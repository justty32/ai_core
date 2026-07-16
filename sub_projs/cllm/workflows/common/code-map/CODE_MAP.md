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
| `src/cabi_response.h` | 回應側對稱四 handler（前三回非 0＝中止）＋輸出結構 | `llm_handlers_t`、`llm_text_handler`／`llm_tool_handler`／`llm_media_handler`／`llm_error_handler`、`llm_tool_call_t`、`llm_media_out_t` |
| `src/cabi_context.h` | 非同步控制（兩個 int，呼叫端自持、無 heap）| `llm_context_t`、`llm_cancel`、`llm_phase`、`llm_phase_t`（IDLE→…→STREAM→DONE/ERROR/CANCELLED）、`llm_status_t` |

### ② C ABI 實作（`namespace cabi_impl`；★ 唯一具名子 namespace，不用匿名 namespace）

| 檔 | 職責 | 關鍵符號 |
|---|---|---|
| `src/cabi_internal.hpp` | 實作共用內部頭（**不外洩**）：context 原子存取＋跨檔函數宣告＋分工表 | `cabi_impl::set_phase`／`cancelled`／`make_request`／`build_body`／`guard_backend_error`／`dispatch_nonstream`／`do_stream` |
| `src/cabi.cpp` | C ABI 出口＋orchestrator：`llm_ask` 分流串流/非串流、收斂例外成 `on_error`；`make_request`（連線設定→`http::Request`）| `llm_ask`、`llm_cancel`、`llm_phase`、`cabi_impl::make_request`（`req_impl` 子 ns）|
| `src/cabi_request.cpp` | **唯一請求真相源**：組 OpenAI 請求 JSON（content 字串或異質陣列、response_format、tools、modalities、取樣、stream）| `cabi_impl::build_body`（`req_impl` 子 ns）|
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
| `src/cli_output.{hpp,cpp}` | 出口面四路回呼：文字 stdout／tool_calls 一行一則 JSON／媒體落檔（路徑吐 stdout）／錯誤 stderr | `output::Sink`（`handlers()`＋`wrote_text`/`media_err` 狀態旗標）|
| `src/main.cpp` | 進入點（Windows `wmain`＋`-municode`；POSIX `main`）| `main`／`wmain` |

### ⑥ 已封存（`archived/`，`git mv` 保史、不在維護鏈）

舊 L0 可獨立呼叫入口——`llm.{hpp,cpp}`（`llm::Client`／`ask`／`from_env`）、`llm_tool`／`llm_media`／`llm_json`（三擴充）、`llm_schema.hpp`（`schema_of<T>()` 反射生 required 的便利層）、舊反射 `cli`、`demo`、舊 `main`。重構時融進 C ABI 實作，**不再對外提供獨立函數**。改現行程式碼**不必讀這裡**；只在追溯「這功能以前怎麼寫的」時參考。

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

- **唯一測試**：`test/cli_smoke.sh`（離線黑箱，31/31）——**端到端**驅動 `build/llm` 打 `test/fixtures/{fake,fake_stream,fake_tool,fake_json,fake_media}/` 的 `file://` 假回應。無單元測試檔，故任何一層改動都靠這支 smoke 從 CLI 端驗（見 [testing](../../testing.md)）。
- **真後端行為**（錯誤路徑／reasoning `max_tokens`／schema `required`）離線驗不出，需使用者實跑（[WAIT_USER](../../../WAIT_USER.md)、[gotchas/backend](../gotchas/backend.md)）。
