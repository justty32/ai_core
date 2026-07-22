# CODE_MAP — cllm 原始碼導航

← [common/conventions](../conventions.md)｜[common/README](../README.md)｜[INDEX](../../../INDEX.md)

修改原始碼前先讀這份：找到相關領域、**只讀該領域列出的檔**，不要漫讀。維護鏈（程式碼 → code map → 文檔）與優先級規則在 [conventions.md](../conventions.md)。技術敘事全貌在 [README.md](../../../README.md)。

> **單一領域**：cllm 全部是「LLM 的 OpenAI 相容接口」一個領域，下分六個關注面。程式碼小（`src/` 約 1.8k 行），故用單一 code map，不再按領域拆子檔。

## 分層總圖

```
純 C 客戶端 ─┐                          llm CLI（unix filter）─┐
             ▼                                                ▼
  cabi.h（C ABI, extern "C"） ◀──鏡像── cabi.hpp（llm::abi, C++ 面）
             │ 唯一入口 llm_ask
             ▼
  C ABI 實作 cabi_impl（按關注點拆）：cabi.cpp／request／response／stream
             │
             ▼
  http.{hpp,cpp}（傳輸管子：WinHTTP／libcurl／file://）
```

## 領域地圖（檔 → 職責 → 關鍵符號）

### ① 對外 C ABI 介面（純 C header，**穩定介面**，改前先想相容性）

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `src/cabi.h` | 總覽傘檔，純 C 可 include；聚合下面四個功能頭 | `llm_ask(ctx, client, req, handlers) → llm_status_t`（**唯一入口**）|
| `src/cabi_client.h` | 呼叫端連線＋取樣設定；數值欄用 `field_mask` 記帳（C 無 `optional`）| `llm_client_t`、`LLM_FIELD_*`（TEMPERATURE/TOP_P/PRESENCE_PENALTY/FREQUENCY_PENALTY/MAX_TOKENS/SEED）、`llm_client_set_field`／`llm_client_has_field` |
| `src/cabi_request.h` | 請求側輸入結構 | `llm_request_t`、`llm_schema_t`、`llm_tool_def_t`、`llm_media_in_t`、`llm_modality_t` |
| `src/cabi_response.h` | 回應側對稱五 handler（前三回非 0＝中止）＋輸出結構 | `llm_handlers_t`、`llm_text_handler`／`llm_tool_handler`／`llm_media_handler`／`llm_error_handler`／`llm_usage_handler`、`llm_tool_call_t`、`llm_media_out_t`、`llm_usage_t` |
| `src/cabi_context.h` | 非同步控制（兩個 int，呼叫端自持、無 heap）| `llm_context_t`、`llm_cancel`、`llm_phase`、`llm_phase_t`（IDLE→…→STREAM→DONE/ERROR/CANCELLED）、`llm_status_t` |

### ② C ABI 實作（`namespace cabi_impl`；★ 唯一具名子 namespace，不用匿名 namespace）

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `src/cabi_internal.hpp` | 實作共用內部頭（**不外洩**）：context 原子存取＋跨檔函數宣告＋分工表 | `cabi_impl::set_phase`／`cancelled`／`make_request`／`build_body`／`guard_backend_error`／`dispatch_nonstream`／`do_stream` |
| `src/cabi.cpp` | C ABI 出口＋orchestrator：`llm_ask` 分流串流/非串流、收斂例外成 `on_error`；`make_request`（連線設定→`http::Request`）| `llm_ask`、`llm_cancel`、`llm_phase`、`cabi_impl::make_request`（`req_impl` 子 ns）|
| `src/cabi_request.cpp` | **唯一請求真相源**：組 OpenAI 請求 JSON（content 字串或異質陣列、response_format、tools、modalities、取樣、stream；裝了 on_usage 的串流多送 stream_options）| `cabi_impl::build_body(c, req, include_usage)`（`req_impl` 子 ns）|
| `src/cabi_response.cpp` | 非串流解析＋錯誤護欄 | `cabi_impl::dispatch_nonstream`、`guard_backend_error`（`resp_impl` 子 ns、`kLenient` opts）|
| `src/cabi_stream.cpp` | 串流 SSE 路徑：逐行解、`on_text` 逐段、tool_calls 按 index 拼、`[DONE]` 收工 | `cabi_impl::do_stream`（`stream_impl` 子 ns）|

### ③ C++ 薄鏡像（header-only，C ABI 的 C++ 面）

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `src/cabi.hpp` | 傘檔：`llm::abi::Client`＋`Status`（`ask()` 融成 class，拆不開故放傘檔）| `llm::abi::Client::ask(req, handlers, ctx)`、`llm::abi::Status`、`llm::abi::Context` |
| `src/cabi_request.hpp` | 鏡像請求型（`const char*`→`std::string`、指標+count→`std::vector`）| `llm::abi::Request`／`Schema`／`ToolDef`／`MediaIn`／`Modality` |
| `src/cabi_response.hpp` | 鏡像 handler（函數指標+`void*`→`std::function`）| `llm::abi::Handlers`／`ToolCall`／`MediaOut` |
| `src/cabi_context.hpp` | 鏡像 context | `llm::abi::Context`（包 `llm_context_t`）|

### ④ 傳輸（兩交付物共用的笨管子）

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `src/http.hpp` | 乾淨介面（系統標頭不外洩）| `http::Request`／`Response`／`OnData`、`http::request`（非串流）、`http::stream`（回 true 中止）|
| `src/http.cpp` | 平台分流實作：Windows=WinHTTP、POSIX=libcurl、`file://` 兩平台特例（讀檔當 200，保住離線 fixture）| `http::request`／`http::stream` 的 `#ifdef _WIN32` 分支 |

### ⑤ `llm` CLI（unix filter；消費 ③ 的 `llm::abi::Client`）

CLI 依三關注點拆檔（cli.cpp 只當 orchestrator）：

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `src/cli.hpp`／`cli_internal.hpp` | 對外介面＋共用常數（退出碼／`kConfigEnvVar`）| `cli::run(args)→int`、`kExit*` |
| `src/cli.cpp` | orchestrator：argv 解析→定 prompt（位置參數×stdin 合體、「-」插入點）→組 Client/Request（含 tool/modality 檔）→ask→退出碼＋SIGINT | `cli::run`、`stdin_is_tty` |
| `src/cli_flags.{hpp,cpp}` | 反射 `Client` 欄位→旗標＋`--help`（型別分派模板在 `.hpp`）| `flags::client_flags`／`print_usage`／`assign_field`／`kebab_flag` |
| `src/cli_config.{hpp,cpp}` | config 三層來源前二層＋檔案/mime/工具定義/媒體落檔小工具 | `config::load_into`／`read_file`／`mime_of`／`ext_of`／`load_tool_def`／`save_media` |
| `src/cli_output.{hpp,cpp}` | 出口面五路回呼：文字 stdout／tool_calls 一行一則 JSON／媒體落檔（路徑吐 stdout）／錯誤 stderr／`--usage` 用量 stderr | `output::Sink`（`handlers()`＋`show_usage`＋`wrote_text`/`media_err` 狀態旗標）|
| `src/main.cpp` | 進入點（Windows `wmain`＋`-municode`；POSIX `main`）| `main`／`wmain` |

### ⑥ 已封存（`archived/`，`git mv` 保史、不在維護鏈）

舊 L0 可獨立呼叫入口——`llm.{hpp,cpp}`（`llm::Client`／`ask`／`from_env`）、`llm_tool`／`llm_media`／`llm_json`（三擴充）、`llm_schema.hpp`（`schema_of<T>()` 反射生 required 的便利層）、舊反射 `cli`、`demo`、舊 `main`。重構時融進 C ABI 實作，**不再對外提供獨立函數**。改現行程式碼**不必讀這裡**；只在追溯「這功能以前怎麼寫的」時參考。

### ⑦ 周邊工具（`tools/`，C++ 模組，進主建置 `-DCLLM_BUILD_TOOLS`）

建在 core 之上、非 `src/` 核心。**入站**用共用微型 server、**出站**重用 `src/http`；翻譯／oauth 用
glaze `json_t`（外部 wire 動態重塑，cllm 自家 ABI 才用反射 struct）。Python 原版在各 `reference/`（不在維護鏈）。

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `tools/common/httpd.{hpp,cpp}` | 微型 HTTP/1.1 **server**（入站；cllm 只有 client）。raw socket、chunked；POSIX＋Winsock（`httpd_impl` 子 ns）| `httpd::Server`（serve_forever/serve_once/port）、`httpd::Request`／`Responder`（send/begin_chunked/write_chunk）|
| `tools/anthropic-proxy/translate.{hpp,cpp}` | OpenAI⇄Anthropic 翻譯（`axl_impl` 子 ns）| `axl::to_anthropic`／`from_anthropic`／`err_to_openai`／`StreamXlate`、`axl::kStructTool` |
| `tools/anthropic-proxy/proxy.cpp` | 代理執行檔：httpd 入站＋src/http 出站＋串流；金鑰轉手不落地 | `main`／`handle` |
| `tools/llm-login/login_abi.h` | 對外 C ABI（仿 cabi.h，**穩定介面**）：與 cllm 聯動入口 | `llm_login_login`／`_refresh`／`_token`、`llm_login_opts_t`／`llm_login_status_t` |
| `tools/llm-login/login.{hpp,cpp}` | C++ 編排（三流程）＋C-ABI 出口＋開瀏覽器 | `llm::login::do_login`／`do_refresh`／`get_token`／`resolve`／`open_browser`、`NeedLogin` |
| `tools/llm-login/oauth.{hpp,cpp}` | PKCE／authorize URL／接 callback／換刷 token（`oauth_impl` 子 ns）| `login::oauth::authorize_url`／`_openrouter`／`capture_code`／`exchange_code`／`refresh`／`redirect_of` |
| `tools/llm-login/store.{hpp,cpp}` | 路徑探測／token 存放 0600／patch cllm config（只動 api_key/endpoint/model）| `login::store::read_json`／`write_file`／`make_token_record`／`is_expired`／`patch_cllm`／`default_path` |
| `tools/llm-login/crypto.{hpp,cpp}` | vendored SHA-256＋base64url＋安全亂數（PKCE S256；`crypto_impl` 子 ns）| `login::crypto::sha256`／`base64url_nopad`／`random_bytes`／`gen_pkce` |
| `tools/llm-login/login_cli.cpp` | `llm-login` CLI | `main`／`cmd_status` |

> ⚠ **低層模組在頂層 ns `login`**（`login::crypto`／`oauth`／`store`）；C++ 入口在 `llm::login`。在
> `llm::login` 內直寫 `store::` 會被解成 `llm::login::store`（不存在）——`login.cpp` 用 `namespace store = ::login::store;` 別名指回。

## 常見任務 → 先讀哪些檔

| 你要做… | 先讀 |
|---------|------|
| 加一個取樣旋鈕（如 `top_k`）| `cabi_client.h`（加欄位＋`LLM_FIELD_`）→ `cabi_request.cpp`（`build_body` 寫進 JSON）→ `cabi.hpp`（鏡像 `optional`）→ CLI 自動長旗標 |
| 改請求 JSON 怎麼組 | `cabi_request.cpp`（唯一真相源）|
| 改回應/串流怎麼解 | 非串流 `cabi_response.cpp`／串流 `cabi_stream.cpp` |
| 動後端錯誤護欄 | `cabi_response.cpp` 的 `guard_backend_error`（＋ `cabi.cpp`／`cabi_stream.cpp` 的攔截點）|
| 加 CLI 固定旗標 | `cli.cpp`（反射旗標自動、固定旗標手寫）|
| 改傳輸/加平台 | `http.cpp`（`#ifdef` 分支）；介面動到才碰 `http.hpp` |
| 動對外 C 結構/簽章 | **先全域 grep 受影響的 CLI／客戶端／`bindings/`（lua・s7）**（鐵律 3），改 `cabi_*.h`＋鏡像 `cabi_*.hpp` 同 commit |

## 測試對應

- **唯一測試**：`test/cli_smoke.sh`（離線黑箱，40/40）——**端到端**驅動 `build/llm` 打 `test/fixtures/{fake,fake_stream,fake_tool,fake_json,fake_media}/` 的 `file://` 假回應。無單元測試檔，故任何一層改動都靠這支 smoke 從 CLI 端驗（見 [testing](../../testing.md)）。
- **真後端行為**（錯誤路徑／reasoning `max_tokens`／schema `required`）離線驗不出，需使用者實跑（[WAIT_USER](../../../WAIT_USER.md)、[gotchas/backend](../gotchas/backend.md)）。
- **周邊工具 tools/**：`llm-login` 有離線測 `build/tools/llm-login-test-offline`（SHA-256/base64url/PKCE/URL/config patch/token 到期，全綠）；`anthropic-proxy` 靠假上游端對端驗（非串流/串流/[DONE]/缺 key→401）。真 OAuth／真 Anthropic 往返需真帳號（WAIT_USER）。
