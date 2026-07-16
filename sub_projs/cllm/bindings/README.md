# bindings — cllm 的腳本語言綁定（Lua／s7 Scheme）

← [cllm 專案根](../README.md)｜[C ABI 參考](../docs/c-abi-reference.md)｜[INDEX](../INDEX.md)

把 `libcllm.so` 的**對外 C ABI**（唯一入口 `llm_ask`）綁進腳本 VM，讓 Lua／Scheme 一句話問 LLM。**只消費穩定 C ABI（連 `-lcllm`）、不碰 cllm 的 CMake/vcpkg**——刻意輕，各夾一個 `build.sh` 就地編、不進主建置。

> API 對齊 galtxt/try_4 的既有慣例（`llm.ask` / `llm-ask` + `on_delta` 串流回呼）。兩邊語意一致，只是換語言的手感。

## 佈局

| 路徑 | 內容 |
|------|------|
| [`lua/`](lua/) | `llm.c`（Lua C 模組）＋`example.lua`＋`build.sh` → 產 `llm.so`（`require("llm")`）|
| [`s7/`](s7/) | `llm_s7.c`（s7 foreign function）＋`host.c`（自帶 host）＋`example.scm`＋`build.sh` → 產 `llm-s7` 執行檔 |

## 建置與試跑（Linux，實測綠）

**Lua**（用系統 lua；`LUA_INC` 可覆寫頭檔目錄）：

```bash
cmake --build --preset linux-debug          # 先有 build/libcllm.so
bash bindings/lua/build.sh                   # → bindings/lua/llm.so
LUA_CPATH="bindings/lua/?.so" lua bindings/lua/example.lua "file://$PWD/test/fixtures/"
```

**s7**（s7 是單檔 Scheme，自備一份，`S7_DIR` 指向含 `s7.c`/`s7.h` 的目錄）：

```bash
export S7_DIR=/path/to/s7                     # 例：某處的 s7-playground
bash bindings/s7/build.sh                     # 把 s7.c 一起編 → bindings/s7/llm-s7
./bindings/s7/llm-s7 bindings/s7/example.scm "file://$PWD/test/fixtures/"
./bindings/s7/llm-s7 -e '(display (llm-ask "你好"))'   # 或直接 -e 表達式
```

兩邊都用 `--endpoint file://…/test/fixtures/` 離線 fixture 自測（不連網、免真後端）；拿掉 `file://` 基底即走內建 `http://localhost:1234/v1/chat/completions`（本機 LM Studio）。

## API（兩邊一致）

**Lua**：`llm.ask(prompt [, endpoint])`｜`llm.ask(prompt, opts)`｜`llm.ask(opts)`
**s7**：`(llm-ask prompt [endpoint])`｜`(llm-ask prompt :key val …)`

回**完整組合後的答案字串**。opts／keyword 欄位（皆選填，未給即不送、交後端默認）：

| 欄位 | Lua key | s7 keyword | 型別 |
|------|---------|-----------|------|
| endpoint | `endpoint` | `:endpoint` | 字串（`file://` 或 `http(s)://…/chat/completions`）|
| api_key | `api_key` | `:api-key` | 字串 |
| model | `model` | `:model` | 字串 |
| timeout_ms | `timeout_ms` | `:timeout-ms` | 整數 |
| 取樣 | `temperature`／`top_p`／`presence_penalty`／`frequency_penalty` | `:temperature`／`:top-p`／`:presence-penalty`／`:frequency-penalty` | 小數 |
| | `max_tokens`／`seed` | `:max-tokens`／`:seed` | 整數 |
| stream | `stream` | `:stream` | 布林 |
| schema | `schema` | `:schema` | JSON Schema 字串（結構化輸出）|
| 串流回呼 | `on_delta` | `:on-delta` | `fn(piece)`；回 `true`/`#t` 中止 |
| 錯誤回呼 | `on_error` | `:on-error` | `fn(msg)` |

**錯誤語意**：Lua 回 `nil, errmsg`；s7 有 `:on-error` 就呼叫它並回 `#f`、否則丟 `(error 'llm-error …)`。

## 範圍與注意（v1，刻意小）

- **只綁 text／schema／stream 那條主路**：`tools`／`media`（vision）／`modalities` 未收——要的話照 [`llm_request_t`](../docs/c-abi-input.md) 補（C ABI 已支援，是 binding 還沒接）。
- **⚠ s7 回呼別丟 Scheme error**：`on-delta`/`on-error` 在 `llm_ask`（C++）執行途中被呼叫，s7 error 以 longjmp 實作、穿 C++ 堆疊是 UB。回呼只做無害的事（display 等）。（Lua 這邊用 `lua_pcall` 包住、安全。）
- **未在 Windows 實測**（`build.sh` 是 bash；Windows 要另配 gcc/mingw 命令與 `.dll`/`.exe` 名）。
- **未接 CMake**（刻意）：bindings 是 C ABI 的下游消費端，維持獨立可編、不進主建置。
- **⚠ 動 C ABI 要想到這兩個下游**：改 `src/cabi.h` 的扁平結構／`llm_ask` 簽章時，bindings 也在鐵律 3 的受影響清單裡。
