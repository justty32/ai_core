# cllm-lab 上手教學 — 用九種語言呼叫 LLM

> 這是 `~/code/cllm-lab` 的教學文檔。想要一頁速查、印出來貼牆的版本 → 開 [`cheatsheet.html`](cheatsheet.html)（瀏覽器直接打開，離線可用）。
> 極簡開工指引在 [`README.md`](README.md)；本檔是把「怎麼用」講清楚的完整版。

---

## 0. 心智模型：一顆 `.so`，一個入口

整套東西的核心只有一件事：

```
libcllm.so  ──  唯一 C ABI 入口  llm_ask()  ──  各語言只是它的一層薄皮
```

- **底層**：`libcllm.so` 把「OpenAI 相容 `/v1/chat/completions` 的一次往返」包成一個 C 函式 `llm_ask()`。
- **各語言 binding**：C / C++ / Lua / Fennel / s7 / Python / Janet / Common Lisp / Go / Shell，全部只是把 `llm_ask()` 用該語言的慣例包一層。**API 形狀十語言一致**，差別只在語法。
- **`llm` CLI**：另一個交付物，一支 unix filter；本質也是 `llm_ask()` 的消費端。

所以你學一次 API，九種語言都會用。本 lab 每個語言資料夾裡的 `play.*` 就是**同一套示範**（基本 ask ／串流／結構化輸出／tools／media／modalities）用九種語言各寫一遍，**直接改 `play.*` 就是開發**。

---

## 1. 一次性設定（換新機器才做）

lab 依賴一份常駐開發環境 `~/dev/cllm/`（放好編譯好的 `.so`、`llm` CLI、各語言的 loader 路徑）。

```bash
# ① 從 repo 建置並安裝到 ~/dev（冪等，重跑只會覆蓋 cllm 部分，不會亂刪）
bash ~/repo/ai_core/sub_projs/cllm/core/install-dev.sh

# ② 健康檢查：九語言一鍵煙霧測，全綠＝環境正常
bash ~/repo/ai_core/sub_projs/cllm/core/test/bindings_smoke.sh
```

之後**每次開工前**，在要跑 lab 的 shell 裡 source 一次環境：

```bash
source ~/dev/cllm/env.sh
```

`env.sh` 設好了 `PATH`（找 `llm`）、`PKG_CONFIG_PATH`（C/C++ 的 `pkg-config cllm`）、`PYTHONPATH`（`import llm`）、各版 Lua 的 `LUA_CPATH`、`CLLM_LISP`、以及 `CLLM_FIXTURES`（離線假回應基底，自測用）。

> ⚠ **編輯器要從 source 過 `env.sh` 的 shell 開**（`code .` / `nvim`）。clangd／gopls／pyright 靠這些環境變數找標頭與套件；沒 source 就開編輯器 → 一片紅波浪線。

---

## 2. 30 秒跑起來

```bash
source ~/dev/cllm/env.sh   # ① 設環境
cd ~/code/cllm-lab/cpp     # ② 挑個語言
bash run.sh                # ③ 跑（離線 fixture，不用開後端、不連網）
```

`run.sh` 會自動 source env、（該編就）編譯、然後用 `$CLLM_FIXTURES` 打離線假回應跑一遍 `play.*`。看到 `[cpp] ask => ...` 一連串輸出就是通了。接著 `nvim play.cpp` 開始改。

**每個語言都是這三步**，只有起手檔名不同：

| 資料夾 | 起手檔 | 跑法 | 編輯器 |
|--------|--------|------|--------|
| `c/`      | `play.c`    | `bash run.sh` | clangd（`compile_flags.txt` 已備）|
| `cpp/`    | `play.cpp`  | `bash run.sh` | clangd（有便利層 `<cllm/llm.hpp>`＋反射糖 `llm_reflect.hpp`）|
| `python/` | `play.py`   | `bash run.sh` | pyright（`pyrightconfig.json` 已備）|
| `janet/`  | `play.janet`| `bash run.sh` | janet-lsp（`project.janet` 已備；native C 模組）|
| `go/`     | `main.go`   | `bash run.sh` | gopls（`go.mod` replace 指 ~/dev；cgo 需先 source env）|
| `lua/`    | `play.lua`  | `bash run.sh` | lua-ls（`.luarc.json` 已備）|
| `fennel/` | `play.fnl`  | `bash run.sh` | —（跑在 lua 5.4）|
| `s7/`     | `play.scm`  | `bash run.sh` | —（`llm-s7` 直譯）|
| `lisp/`   | `play.lisp` | `bash run.sh` | SLIME 照常；需 quicklisp（CFFI＋shasht）|
| `shell/`  | `play.sh`   | `bash run.sh` | —（`llm` CLI 是 unix filter）|

---

## 3. API 全貌：一套概念、九種語法

所有 binding 共通的概念層：

- **`ask(prompt[, endpoint], 選項…)`** → 回**完整答案字串**。這是九成場景。
- **`system`**：system role 指示（純文字）——非空就在 user 訊息前插一則 `role:"system"`。命名照各語言慣例（Lua/Python `system=`；s7/CL/Janet `:system`；Go `cllm.System(...)`；C/C++ 設 `Request.system`；CLI `--system`）。
- **`on_delta`**：給了就是**串流**，逐段回呼（回 truthy 可中止），回傳仍是聚合後整段。
- **`schema`**：給一段 JSON Schema，要求**結構化輸出**（回應是 JSON 字串，自己解）。C++ 另有 `ask_as<T>`：struct 進 struct 出，schema 由反射自動生、回應自動解回 struct，零手刻 JSON。
- **`tools` + `on_tool`**：送工具定義（`{name, description, parameters}`，parameters 是 JSON Schema 字串），模型要求呼叫時 `on_tool` 收到 `{name, arguments}`（arguments 是 JSON 字串，自己解）。**單輪**——要不要真的執行、要不要多輪由你決定。
- **`media` + `modalities`**：`media` 掛輸入媒體（data URI 或 URL）；`modalities` 要求輸出模態（如 `audio`）＋其參數。
- **`on_media`**：模型產出的媒體（`{mime, bytes}`）。

下面每個語言給「基本 ask」＋「串流」兩句話當語感錨點；完整六個示範看該資料夾的 `play.*`。

### C — `#include <cllm/cabi.h>`
最底層，一切都是 struct 填欄位。JSON 要另外配 jansson。
```c
llm_client_t c = {0}; c.endpoint = "http://localhost:1234/v1/chat/completions";
llm_request_t r = {0}; r.prompt = "你好";
llm_handlers_t h = {0}; h.on_text = on_text;   /* on_text(text,n,userdata) 逐段累積 */
llm_ask(NULL, &c, &r, &h);                      /* 回 llm_status_t，LLM_OK 為成功 */
```
編：`cc play.c $(pkg-config --cflags --libs cllm jansson) -o example`

### C++ — `#include <cllm/llm.hpp>`（便利層）
最好用的一層：`Result<std::string>`（失敗走 `.error().message`，不混空字串）、`ask_as<T>` 反射。
```cpp
llm::Client c; c.endpoint = "http://localhost:1234/v1/chat/completions";
auto r = c.ask("你好");                          // Result<string>：if(r) *r 否則 r.error()
c.ask("數到五", {.stream=true, .on_delta=[](std::string_view sv){ /*…*/ return false; }});
auto ch = llm::ask_as<Character>(c, "給我角色");  // struct 進 struct 出（struct 須放檔案層級）
```
編：`g++ -std=c++23 play.cpp $(pkg-config --cflags --libs cllm) -o example`
反射糖（`<cllm/llm_reflect.hpp>`）：`ask_as<T>`／`make_tool<Args>`／`args_as<Args>`。⚠ 給反射的 struct 要在 namespace scope，函式內 local struct 反射不了。

### Python — `import llm`
```python
llm.ask("你好", "http://localhost:1234/v1/chat/completions")
llm.ask("數到五", endpoint=ep, stream=True, on_delta=lambda p: (print(p, end=""), False)[1])
obj = json.loads(llm.ask("給我角色", endpoint=ep, schema='{"type":"object"}'))
# system="你是貓", tools=[{"name","description","parameters"}], on_tool=fn(call), on_media=fn(m)；錯誤丟 llm.LLMError
```

### Janet — `(import llm)`（native C 模組，`:keyword` 參數）
純 FFI 表達不了 cllm 的三參數有回傳回呼，故走 native C 模組（跟 lua/s7 同型態）。opts 用**連字號 keyword**（`:on-delta`／`:api-key`／`:max-tokens`）。
```janet
(llm/ask "你好" (ep "…"))                                    # 第二個位置參數＝endpoint
(llm/ask "數到五" :endpoint ep :stream true :on-delta (fn [p] (prin p) false))
# :system "你是貓"（system role 指示）；:schema json → 回 JSON 字串，用 (import spork/json) → (json/decode …) 解
# :tools [{:name … :description … :parameters json}]；:on-tool (fn [call] (call :name)(call :arguments))
# :on-media (fn [m] (m :mime)(m :bytes))；:media [{:url …}]；:modalities [{:name "audio" :config json}]
```
失敗時給了 `:on-error` 回 `nil`，否則丟 janet error（可 `(try … ([e] …))`）。回呼裡 janet error 已被 `janet_pcall` 收住、乾淨中止後上拋，安全。

### Go — `import "cllm"`（functional options）
```go
ans, err := cllm.Ask("你好", cllm.Endpoint(url))              // (string, error)
cllm.Ask("數到五", cllm.Endpoint(url), cllm.Stream(),
         cllm.OnDelta(func(p string) bool { /*…*/ return false }))
// cllm.System / cllm.Schema / cllm.Tools(ToolDef{…}) / cllm.OnTool / cllm.OnMedia / cllm.MediaIn / cllm.Modalities
```
⚠ cgo 需先 `source env.sh`；`go.mod` 的 replace 已把 `cllm` 指到 ~/dev。

### Lua — `require("llm")`（table 傳參）
```lua
llm.ask("你好", endpoint)                                     -- 短式
llm.ask{ prompt="數到五", endpoint=ep, stream=true, on_delta=function(p) io.write(p) return false end }
-- 完整選項：system / schema / tools={{name,description,parameters}} / on_tool / on_media / media / modalities
-- JSON 用 dkjson（require "dkjson"）
```

### Fennel — `(require :llm)`
跟 Lua 完全相同（跑在 lua 5.4 的 `llm.so`），只差 Lisp 語法。**⚠ table 鍵一律用底線**：`:on_delta` `:on_tool`，不是 `:on-delta`。
```fennel
(llm.ask "你好" (ep "…"))
(llm.ask {:prompt "數到五" :endpoint ep :stream true :on_delta (fn [p] (io.write p) false)})
```

### s7 Scheme — `(llm-ask …)`（關鍵字參數）
```scheme
(llm-ask "你好" :endpoint ep)
(llm-ask "數到五" :endpoint ep :stream #t :on-delta (lambda (p) (display p) #f))
; :system "你是貓"（system role 指示）；:tools (list (list "name" "desc" "params-json"))；:on-tool (lambda (id name arguments) …)
; :on-media (lambda (mime bytes) …)；s7 無原生 json → 抽欄位 shell-out 給 jq
```
跑：`llm-s7 play.scm "$CLLM_FIXTURES"`。**⚠ 回呼裡別丟 error**——longjmp 穿過 C++ 是 UB；只做 `set!` 存值、把 JSON 解析／檔案 I/O 挪到 `llm-ask` 呼叫之後。

### Common Lisp — `(cllm:ask …)`
```lisp
(cllm:ask "你好" (ep "…"))
(cllm:ask "數到五" :endpoint ep :stream t :on-delta (lambda (p) (write-string p) (finish-output) nil))
; :system "你是貓"（system role 指示）；:tools (list (list :name "…" :description "…" :parameters "…"))；:on-tool (lambda (call) (getf call :name/:arguments))
; :on-media (lambda (m) (getf m :mime/:bytes))；JSON 用 shasht（quicklisp）
```
跑：`sbcl --script play.lisp`。需先 `(load (sb-ext:posix-getenv "CLLM_LISP"))`（env.sh 已設 `CLLM_LISP`）。

### Shell — `llm` CLI
把上面全部包成一支 unix filter；沒有 SDK，就是管線＋jq。詳見下一節。

---

## 4. `llm` CLI 用法（unix filter）

`llm` 是一支標準過濾器：prompt 走**位置參數或導管 stdin（可合體）**，答案吐 stdout、診斷吐 stderr。

### prompt 來源規則

| 給了什麼 | prompt 是 |
|----------|-----------|
| 只有位置參數 | 位置參數（空格拼接）|
| 只有導管 stdin | stdin 整段 |
| 兩者都有、無 `-` | 位置參數 ＋ 空行 ＋ stdin（**指令在前、資料在後**）|
| 位置參數含 `-` | `-` 被 stdin 整段替換（**明指插入點**）|

### 日常範例
```bash
llm 用一句話介紹你自己                        # 位置參數當 prompt
echo "數到五" | llm --stream                  # 讀 stdin；--stream 逐段即時印
cat report.txt | llm 總結這份                  # prompt＋stdin 合體（指令在前、資料在後）
git diff | llm 把 - 寫成 commit 訊息           # 「-」明指 stdin 插入點
llm 這張圖是什麼 --image ./cat.jpg             # --image 可重複，mime 由副檔名推
llm 生成一個傲嬌角色 --schema ./char.json      # --schema：JSON Schema → 結構化輸出
llm 東京天氣如何 --tool ./get_weather.json     # tool_calls 一行一則 JSON 吐 stdout
llm 說你好 --modality audio=./cfg.json --media-out ./out   # 產出媒體落檔、路徑吐 stdout
```

### 常用旗標
| 旗標 | 說明 |
|------|------|
| `--stream` | 串流逐段吐 stdout |
| `--image <檔>` | 輸入媒體（可重複；mime 由副檔名推：png/jpg/gif/webp/wav/mp3）|
| `--schema <檔>` | JSON Schema 檔 → 結構化輸出 |
| `--tool <檔>` | 工具定義檔（可重複）`{name,description,parameters}` |
| `--modality <名[=檔]>` | 要求輸出模態（可重複）如 `audio` 或 `audio=cfg.json` |
| `--media-out <目錄>` | 產出媒體落檔目錄（須先存在）|
| `--endpoint <url>` | 後端（預設 `http://localhost:1234/v1/chat/completions`）|
| `--model <s>` | 模型 id（本地填 `local-model`；雲端必填真 id）|
| `--api-key <s>` | Bearer 金鑰 |
| `--config <檔>` | 扁平 JSON 設定檔 |
| `--temperature` `--top-p` `--max-tokens` `--seed` `--timeout-ms` … | 取樣／連線旋鈕（由 `Client` 欄位反射生成，以 `llm --help` 為準）|

> 連線／取樣旗標是從 `llm::abi::Client` 的欄位**反射**出來的：要多一個旋鈕＝在 struct 加一個欄位，旗標與 `--help` 自動長出來。所以**永遠以 `llm --help` 實際輸出為準**。

### 退出碼
`0` 成功｜`1` 用法錯（未知旗標、缺 prompt、檔讀不到、JSON 壞…）｜`2` 請求失敗（連不上、後端回錯、落檔失敗）｜`130` Ctrl-C 取消。

---

## 5. 接後端 / 登入

lab 預設打**離線 fixture**（`file://…`，回歸測試用、不連網）。要接真的模型有三條路。

### (a) 本機 LM Studio（最省事）
在 LM Studio 載一個模型、開 server（預設 `:1234`），然後把 endpoint 指過去：
```bash
llm 用一句話介紹你自己 --endpoint http://localhost:1234/v1/chat/completions --model local-model
```
在 `play.*` 裡，把 `endpoint` 改成同一個 URL 即可（或跑 `run.sh` 時傳這個 base 進去）。
- `model`：LM Studio 收 `local-model`（用當前已載入模型）。
- `timeout_ms`：真後端（尤其 reasoning 模型）單次可能等數十秒，config 設 `120000` 寬裕些。
- ⚠ **reasoning 模型的 `max_tokens` 陷阱**：思考鏈與答案共用同一份預算，設太小（如 600）會讓 reasoning 吃光、`content` 變空。打 reasoning 模型時刻意不設 `--max-tokens`。

### (b) 雲端 Anthropic —— 走本機轉發代理 `anthropic-proxy`
cllm 只會說 OpenAI 協定，Anthropic 收的是 Messages API。`anthropic-proxy` 起一支本機代理在中間翻譯（無狀態、金鑰轉手不落地），讓 cllm 直連 `api.anthropic.com`：
```bash
cd ~/repo/ai_core/sub_projs/cllm
cmake --build --preset linux-debug --target anthropic-proxy
./build/tools/anthropic-proxy                        # 或 tools/anthropic-proxy/service/proxyctl.sh start
cp tools/anthropic-proxy/configs/opus.json ~/.config/llm/config.json   # 填入你的 sk-ant- key
llm 哈囉                                              # llm 讀 ~/.config/llm/config.json → 打 proxy → Anthropic
```
（⚠ 真 `sk-ant-` 往返尚未實跑驗證，屬待使用者項——第一次跑請留意真實回應欄位。）

### (c) OAuth 帳號登入 —— `llm-login`
用授權碼＋PKCE 帳號登入換 token 餵給 cllm 的 `api_key`、到期自動 refresh。**最快從 OpenRouter 驗**（免註冊 OAuth client）：
```bash
cd ~/repo/ai_core/sub_projs/cllm
cmake --build --preset linux-debug --target llm-login
cp tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json
./build/tools/llm-login login          # headless 加 --no-browser，另開瀏覽器貼網址
# 拿到 key → 寫進 config.json → 之後 llm 裸跑就帶得上 bearer
```
`llm-login` 設計成 cllm 401 → harness 攔 → 自動開瀏覽器登入 → patch config → 重試，**cllm 本身零改動**。

### 供應商速記
| 供應商 | 怎麼接 |
|--------|--------|
| **OpenRouter** | `llm-login`，免註冊 client，最省事 |
| **Gemini / Azure / GitHub** | `llm-login`（各自先備好 OAuth Client / Entra app+tenant / GitHub OAuth App）|
| **DeepSeek / OpenAI** | 直接填 `--api-key` / config 的 `api_key`（OpenAI 相容，不需 proxy）|
| **Anthropic** | 走 `anthropic-proxy`（協定不相容，需翻譯）|
| **本機 LM Studio** | `--endpoint localhost:1234`，`--model local-model`，免 key |

---

## 6. 建置 / 安裝細節

lab 本身不建置——它吃的是 `~/dev/cllm/` 裡裝好的產物。要重建或改 cllm 本體時：

```bash
cd ~/repo/ai_core/sub_projs/cllm
cmake --preset linux-debug          # 首次配置（vcpkg 自動裝 glaze，約數秒）
cmake --build --preset linux-debug  # 建 libcllm.so、llm CLI、tools（proxy/login）
bash install-dev.sh                 # 把產物冪等安裝進 ~/dev/cllm
```

- **相依刻意極簡**：C++ 側只用 vcpkg 的 **glaze**（header-only 反射式 JSON↔struct）＋系統 **libcurl**。要再加庫＝`vcpkg.json` 的 `dependencies` 加名字 → `find_package`＋`target_link_libraries` → 重配置。
- **preset**：`linux-*`（Manjaro 實測）／`mingw-*`（Windows，路徑釘在 `CMakePresets.json`）。換機改那幾處路徑即可。
- **Windows**：socket/bcrypt 已用 `#ifdef` 包好、POSIX 已測，但**尚未在 Windows 實建**（open 尾巴）。
- **tools 下游可 link**：`cmake --install` 會裝 `liblogin.so`＋`login_abi.h`＋pkgconfig，下游 `pkg-config --cflags --libs llm-login` 即可 include／link。

---

## 7. 疑難排解

| 症狀 | 解法 |
|------|------|
| 編輯器一片紅波浪線（找不到標頭／套件）| 沒 source env.sh 就開了編輯器。**從 source 過 env.sh 的 shell** 重開（`source ~/dev/cllm/env.sh && code .`）|
| `run.sh` 說找不到 `llm` / `libcllm.so` | 環境沒裝好或沒 source。跑 `bash ~/repo/ai_core/sub_projs/cllm/core/install-dev.sh` 再 `source ~/dev/cllm/env.sh` |
| 不確定環境是否健康 | `bash ~/repo/ai_core/sub_projs/cllm/core/test/bindings_smoke.sh`（九語言一鍵，全綠＝正常）|
| 打真後端 `content` 是空的 | reasoning 模型把 `max_tokens` 吃光了——別設 `--max-tokens`（見 §5a）|
| Fennel 回呼沒觸發 | table 鍵用了連字號。改底線：`:on_delta` 不是 `:on-delta`（§3 Fennel）|
| s7 跑一半崩潰 | 回呼裡丟了 error／做了檔案 I/O。回呼只 `set!` 存值，其餘挪到 `llm-ask` 之後（§3 s7）|
| Go 編譯 cgo 報錯 | 先 `source ~/dev/cllm/env.sh` 再 `go run .`（cgo 靠環境變數）|

---

## 真相源與去向

- 本 lab 的 `play.*` 抄自 `~/repo/ai_core/sub_projs/cllm/core/bindings/<lang>/example.*`。**lab 是暫存遊樂場**——值得留的成果記得搬回 repo。
- 完整權威文檔：
  - CLI 手冊 → `~/repo/ai_core/sub_projs/cllm/core/docs/cli-manual.md`
  - 建置／依賴 → `.../docs/setup.md`、跨平台 → `.../docs/platform.md`
  - C ABI 參考 → `.../docs/c-abi-reference.md`、C++ 鏡像 → `.../docs/cpp-mirror-reference.md`
  - binding 選項表／注意事項 → `.../bindings/README.md`
  - 周邊工具（proxy／login）→ `.../tools/README.md`、`.../tools/INTEGRATION.md`
