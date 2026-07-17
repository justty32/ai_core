# llm-login —— 用「帳號登入（OAuth）」餵 token 給 cllm（C-ABI 共享庫＋CLI）

← [tools](../README.md)｜[cllm INDEX](../../INDEX.md)｜對照：[anthropic-proxy](../anthropic-proxy/README.md)

> **補上 cllm 缺的那一段。** cllm 不懂「登入」——它只把 config 的 `api_key` 塞進
> `Authorization: Bearer`。本工具做 OAuth 2.0 **授權碼＋PKCE** 登入、拿 token、到期自動
> refresh，把當前有效 `access_token` 寫進 cllm 的 `config.json`。**cllm 核心一行不動。**

## 就跟 cllm 一樣的模組形態（C-ABI lib）

純 C++23，建成 **`liblogin.so`（對外 C ABI，`login_abi.h`）＋`llm-login` CLI**——正如 cllm 是
`libcllm.so`＋`llm` CLI。C-ABI 的用意＝**與 cllm 聯動**（見下）。零新依賴：入站接 callback 用共用
[`../common/httpd`](../common/httpd.hpp)、出站換 token 重用 [`../../src/http`](../../src/http.hpp)、
PKCE 用 vendored SHA-256（[crypto.cpp](crypto.cpp)）、JSON 用 glaze。

> Python 原版保留在 [`reference/`](reference/)（一比一對照）；C++ 版為現行實作。

## 聯動場景（C-ABI 為此而生）

理想流程——**cllm 零改動**，靠外層 harness 串起來：

```
cllm 呼叫 → 401（api 失效／沒登入）→ harness 偵測 → llm_login_login(&opts)
   → 自動開瀏覽器 → 用戶登入 → 換 token → patch cllm config.json → harness 重試 cllm
```

C-ABI（[login_abi.h](login_abi.h)，記憶體約定同 [cabi.h](../../src/cabi.h)：不配置要呼叫端釋放的東西）：

```c
llm_login_status_t llm_login_login(const llm_login_opts_t *opts);    /* 開瀏覽器→登入→patch config */
llm_login_status_t llm_login_refresh(const llm_login_opts_t *opts);  /* refresh_token 換新 */
long               llm_login_token(const llm_login_opts_t *opts, char *buf, size_t len); /* 取有效 token */
```

`opts` 帶三個路徑（provider/token/config，NULL＝用預設探測）＋`open_browser`＋`err` buffer。回傳把
「沒登入（NEED_LOGIN）」跟「一般錯誤」分開，讓 harness 知道該不該觸發登入。

## 主流供應商：能接的與接不了的

**OAuth 帳號登入換 API 存取**分兩派，不是每家都能接：

**能接**（真・程式化 OAuth，`providers/` 有現成 preset）——皆 OpenAI-compatible＋Bearer：

| Preset | 流程 | 你要先備 |
|--|--|--|
| [openrouter](providers/openrouter.json) | PKCE，**免註冊 client**，換回**不過期** key | 一個 OpenRouter 帳號 |
| [openrouter-deepseek](providers/openrouter-deepseek.json) | 同上，`cllm_model` 預設 deepseek | 同上 |
| [google-gemini](providers/google-gemini.json) | 標準 OAuth | Google Cloud OAuth Client ID＋scope |
| [azure-openai](providers/azure-openai.json) | 標準 OAuth（Entra ID） | Entra app＋tenant＋角色 |
| [github-models](providers/github-models.json) | 標準 OAuth | GitHub OAuth App |

**接不了**（key-only，無正規 OAuth）：**DeepSeek／OpenAI 直連**只有 `sk-` key → **直接填 config**
即可（OpenAI-compat＋Bearer，cllm 本就接得上，不用本工具）；**Anthropic 直連**是 `x-api-key`＋非
OpenAI 格式 → 用姊妹工具 [anthropic-proxy](../anthropic-proxy/README.md)。

> ⚠ **最省事推薦 OpenRouter**：免註冊 OAuth client、換回不過期 key、一個 endpoint 通吃（含 Claude）。

## 建置

隨 cllm 一起建（`-DCLLM_BUILD_TOOLS`，預設 ON）：

```sh
cd sub_projs/cllm && cmake --preset linux-debug
cmake --build --preset linux-debug --target llm-login   # → build/tools/{liblogin.so, llm-login}
```

## 下游如何 link（就跟 cllm 一樣是可取用的模組）

`cmake --install` 把 llm-login 裝成下游可 include/link 的模組——沿用 cllm 那一套（`configure_file`→`.pc`，非 `install(EXPORT)`），保持一致。裝到某前綴：

```sh
cmake --preset linux-debug -DCMAKE_INSTALL_PREFIX=$HOME/dev   # 或任意 prefix
cmake --build   --preset linux-debug
cmake --install build
```

裝出來的產物（`<prefix>/` 下）：

| 產物 | 落點 | 用途 |
|--|--|--|
| `liblogin.so` | `lib/` | 下游 link 的 C-ABI 共享庫（帶 INSTALL_RPATH 指自己 libdir，免 `LD_LIBRARY_PATH`）|
| `login_abi.h` | `include/cllm/` | 對外 C ABI 頭（與 cllm 頭同放）|
| `llm-login.pc` | `lib/pkgconfig/` | pkg-config（模組名 `llm-login`，`Libs: -llogin`）|
| `llm-login` | `bin/` | CLI（順帶裝，同 cllm 裝 `llm`）|

下游純靠 pkg-config 就能 include＋link（`<prefix>/lib/pkgconfig` 進 `PKG_CONFIG_PATH`）：

```sh
gcc consumer.c $(pkg-config --cflags --libs llm-login) -o consumer
# --cflags → -I<prefix>/include -I<prefix>/include/cllm ；--libs → -L… -Wl,-rpath,… -llogin
```

```c
#include <login_abi.h>
int main(void){ llm_login_opts_t o={0}; return llm_login_login(&o); }  /* 三支 C-ABI 皆匯出 */
```

## 用法（CLI）

先備 provider 設定——挑一個 preset 複製、填 `_notes` 交代的空：

```sh
cp providers/openrouter.json ~/.config/llm/oauth.json     # 最省事；OpenRouter 幾乎不用改
# 或用環境變數指到 preset：export LLM_OAUTH_PROVIDER=$PWD/providers/openrouter.json
```

```sh
llm-login login      # 開瀏覽器登入一次 → 拿 token、patch cllm config
llm-login status     # 看狀態（不外洩 token 本身）
llm-login refresh    # 手動用 refresh_token 換新
llm-login token      # 印當前有效 token（快過期自動 refresh）——給腳本 $(...)
# 旗標：--provider P / --token T / --config C 覆寫路徑；--no-browser 只印網址不自動開（headless）
```

登入後裸跑 cllm 就帶著 bearer（config 的 `api_key` 已被寫入）。短命 token 也可即時取：

```sh
llm 你好 --api-key "$(llm-login token)"   # 過期會自動 refresh 再印
```

## 檔案

| 檔 | 是什麼 |
|--|--|
| [login_abi.h](login_abi.h) | 對外 C ABI（仿 cabi.h）：login／refresh／token；聯動入口 |
| [login.{hpp,cpp}](login.cpp) | C++ 編排（三流程）＋C-ABI 出口＋開瀏覽器（xdg-open／ShellExecute） |
| [oauth.{hpp,cpp}](oauth.cpp) | 協定層：PKCE、authorize URL（標準＋OpenRouter）、接 callback、換／刷 token |
| [store.{hpp,cpp}](store.cpp) | 狀態層：路徑探測、token 存放（0600）、只 patch cllm config 的 api_key/endpoint/model |
| [crypto.{hpp,cpp}](crypto.cpp) | vendored SHA-256＋base64url＋安全亂數（PKCE S256） |
| [login_cli.cpp](login_cli.cpp) | `llm-login` CLI |
| [test_offline.cpp](test_offline.cpp) | 離線測（SHA-256／base64url／PKCE／URL／config patch／token 到期）|
| [login.pc.in](login.pc.in) | pkg-config 範本（`configure_file`→`llm-login.pc`；下游 link 用，見「下游如何 link」）|
| [providers/](providers/) | 主流供應商現成 preset（每檔 `_notes` 交代要填什麼） |
| [oauth.example.json](oauth.example.json) | 供應商中立範本（接上表以外的家、自己填全部時用） |
| [reference/](reference/) | Python 原版（封存為參考，非現行實作） |

## 三個檔的落點（都在 `~/.config/llm/`，可用 opts/env 改）

| 檔 | 誰寫 | 內容 |
|--|--|--|
| `oauth.json` | 使用者手填（或 `LLM_OAUTH_PROVIDER` 指定） | provider 設定 |
| `oauth_token.json` | 本工具（0600） | access／refresh／expires_at |
| `config.json` | cllm 的設定；本工具**只 patch** api_key/endpoint/model（或 `LLM_CLI_CONFIG` 指定） | 其餘鍵原樣保留 |

## 驗證

```sh
./build/tools/llm-login-test-offline    # 離線 24 條，應全綠
```

真 OAuth 往返（`login`／`refresh` 打真 endpoint＋開瀏覽器）本質要真供應商＋真帳號，Claude 跨不過去
（見 [WAIT_USER](../../WAIT_USER.md)）。免瀏覽器的整條登入鏈（接 callback→換 token→存檔→patch）已用假
token 端對測過。

## 安全與條款

金鑰走呼叫端提供的 buffer／0600 檔，不外洩。只對「有提供 OAuth 給 API 存取」的供應商合法適用；
拿去套消費級網頁訂閱的 session 屬濫用，別這樣用。指向哪個 endpoint、是否符合條款，是使用者的責任。
