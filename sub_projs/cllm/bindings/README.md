# bindings — 各語言用 cllm（含直接用 C ABI 的 C/C++）

← [cllm 專案根](../README.md)｜[C ABI 參考](../docs/c-abi-reference.md)｜[INDEX](../INDEX.md)

把 `libcllm.so` 的**對外 C ABI**（唯一入口 `llm_ask`）綁進各語言，一句話問 LLM。**只消費穩定 C ABI、不碰 cllm 的 CMake/vcpkg**（刻意輕）。API 對齊 galtxt/try_4：`ask` + `on_delta` 串流回呼，回完整答案字串（C++ 另疊了一層便利層，見下表）。

## 常駐開發環境（推薦入口）

**[`../install-dev.sh`](../install-dev.sh)** 一鍵把 cllm 裝成常駐前綴（預設 `~/dev`，像 `/usr/{include,lib,bin}`）＋搭好全部語言環境，**可重現**（重跑即冪等覆蓋；⚠ 勿 `rm -rf ~/dev`——prefix 裡可能還有別的東西）：

```bash
bash install-dev.sh            # → ~/dev/{include,lib,bin,share}
source ~/dev/cllm/env.sh       # 設好 PATH／pkg-config／lua cpath／LIBCLLM／CLLM_LISP…
# 逐語言範例（離線 fixture，$CLLM_FIXTURES 由 env.sh 設）：
bash   ~/dev/share/cllm/examples/shell/example.sh   "$CLLM_FIXTURES"
python3 ~/dev/share/cllm/examples/python/example.py "$CLLM_FIXTURES"
```

各語言用法＋JSON 庫對照見 `~/dev/share/cllm/README.md`（由腳本生成）。

## 各語言（原始碼在此，install-dev.sh 建置/複製）

| 語言 | 產物／用法 | JSON 庫 | 原始碼 |
|------|-----------|---------|--------|
| C    | `cc x.c $(pkg-config --cflags --libs cllm jansson)`；`<cllm/cabi.h>` | jansson | [c/](c/) |
| C++  | `g++ -std=c++23 …`；`<cllm/llm.hpp>`＋`<cllm/llm_reflect.hpp>`（便利層，`namespace llm`）；底層仍可用 `<cllm/cabi.hpp>`（`llm::abi`）| glaze | [cpp/](cpp/) |
| Lua  | `require("llm")`（5.5＝`lua`、5.4＝`lua5.4`）| dkjson | [lua/](lua/) |
| Fennel | `(require :llm)`（跑在 lua 5.4）| dkjson | [fennel/](fennel/) |
| s7   | `llm-s7 script.scm`（`llm-ask` 內建）| jq（shell-out）| [s7/](s7/) |
| Python | `import llm`（純 ctypes、零編譯）| stdlib `json` | [python/](python/) |
| Janet | `(import llm)`（native C 模組，非純 FFI）| spork/json | [janet/](janet/) |
| Common Lisp | `(load CLLM_LISP)`→`cllm:ask`（CFFI）| shasht（quicklisp）| [lisp/](lisp/) |
| Go | `import "cllm"`（cgo，pkg-config cllm）| stdlib `encoding/json` | [go/](go/) |
| Shell | `llm 你好`（unix filter CLI）| jq | [shell/](shell/) |

每個 example 都示範：基本 `ask`、串流、`schema`→JSON 解析、`tools`＋on_tool、media 輸出（on_media）、media 輸入＋`modalities` 搬運、以及從該語言 shell-out 呼叫 `llm` CLI。

## 煙霧測試（離線、一鍵）

```bash
bash ../test/bindings_smoke.sh     # 全語言輪流跑（前置：install-dev.sh 裝好 ~/dev）
bash <lang>/smoke.sh               # 單語言（自動 source ~/dev/cllm/env.sh）
```

每個語言資料夾各有一個 `smoke.sh`（跑該語言 example、比對關鍵標記），master 只是輪流呼叫它們。

## 映像/部署（Lisp 家族：s7／Common Lisp／Fennel）

把綁定＋你的碼做成**映像**（image）＝快啟動、可當 CLI/lib、還能執行期改。各語言能力不同，見各自 `image/`：

| | 記憶體映像 dump | 產獨立執行檔 | 執行期修改 | 改動存回映像 | 細節 |
|---|---|---|---|---|---|
| **Common Lisp (SBCL)** | ✅ `save-lisp-and-die` | ✅ exe/core | ✅ | ✅ | [lisp/image/](lisp/image/README.md) |
| **s7** | ❌ | ✅ 烤進 C host | ✅ | ❌（重編）| [s7/image/](s7/image/README.md) |
| **Fennel/Lua** | ❌ | ✅ AOT `.lua` | ✅ | ❌（重編）| [fennel/image/](fennel/image/README.md) |

## 選項（三種寫法，語意一致）

- **Lua / Python**：`ask(prompt[, endpoint], opts…)`，opts 用 **snake_case**（`on_delta`／`api_key`／`max_tokens`）。
- **s7 / CL / Janet**：`(llm-ask prompt [endpoint] :key val …)`／`(cllm:ask …)`／`(llm/ask prompt [endpoint] :key val …)`，**hyphen keyword**（`:on-delta`／`:api-key`／`:max-tokens`）。
- **Go**：`Ask(prompt, opts...) (string, error)`，functional options（`cllm.Endpoint(url)`／`cllm.Temperature(0.7)`／`cllm.Stream()`／`cllm.OnDelta(fn)`）；錯誤走回傳的 `error`。
- **C++**：`ask(prompt, {.stream=…, .on_delta=…})` 回 `std::expected<std::string, Error>`；錯誤統一走 `expected`（不混 throw／空字串），要一行爽寫可 `.value()` 就地拋。
- 欄位：`endpoint`／`api_key`／`model`／`timeout_ms`／`temperature`／`top_p`／`presence_penalty`／`frequency_penalty`／`max_tokens`／`seed`／`stream`／`schema`／`on_delta`／`on_error`（皆選填）。回完整答案字串。
- **進階欄位（v1 已全語言收齊）**：`tools`（name/description/parameters）＋`on_tool`（收 id/name/arguments，回真值＝中止）、`media`（url 或 bytes＋mime）、`modalities`（name＋config JSON）、`on_media`（收 mime/bytes）。命名照各語言慣例（snake_case／hyphen keyword／functional options）。⚠ tools 是**單輪**：執行工具與是否回送由呼叫端決定（C ABI 無多輪 loop）。

## 注意（v1，刻意小）

- **⚠ Fennel 用 lua 的鍵名**：fennel 呼叫的是 lua binding，table 鍵要用**底線** `:on_delta`（不是 fennel 慣用的 `:on-delta`——那會變成 `"on-delta"` 對不上）。
- **⚠ Lua 模組 ABI 綁 lua 版本**：`lua`（5.5）與 `fennel`（5.4）各需一份 `llm.so`（install-dev.sh 兩版都編，env.sh 用 `LUA_CPATH_5_4/5_5` 分流）。
- **⚠ s7 回呼別丟 error**：`:on-delta`/`:on-error` 在 `llm_ask`（C++）途中被呼叫，s7 error 以 longjmp 實作、穿 C++ 是 UB。回呼只做無害的事。（Lua/Python/CL/Janet 有包 pcall/handler-case，安全。）
- **⚠ Janet 走 native C 模組、非純 FFI**：Janet 的 `ffi/trampoline` 把回呼簽名寫死成 `void trampoline(void*, void*)`，表達不了 cllm 的 `int on_text(const char*, size_t, void*)`（三參數＋int 回傳），故跟 lua/s7 一樣寫 C 橋接（`janet_pcall` 回呼 JanetFunction，回真值＝中止；回呼 janet error 已被 pcall 收住）。`bash build.sh` 直接 gcc 編（需 `JANET_INC` 指到含 `janet.h` 的目錄，預設 `~/.local/include/janet`），不必 jpm。**⚠ JANET_PATH 併預設 tree**：env.sh 的 `JANET_PATH="$DEV/lib/janet:<janet syspath>"`——前者放裝好的 `llm.so`、後者留給 example 的 `spork/json`／`spork/sh`（janet 的 JANET_PATH 吃 `:` 分隔多根）。
- **Common Lisp 需 quicklisp**（載 CFFI／shasht）；`~/.sbclrc` 已載 quicklisp 即可，`--script` 下 cllm.lisp 會自載 `~/quicklisp/setup.lisp`。
- **Go 走 cgo**：`#cgo pkg-config: cllm` 於 `go build` 時跑 pkg-config，故需先 `source env.sh`（設 `PKG_CONFIG_PATH`）。回呼用 `cgo.Handle` 把 Go closure 帶進 C，schema struct 用 `runtime.Pinner` pin（Go 1.21+ 內嵌 Go 指標規則）。
- **s7 原始碼已 vendor 在 repo**（`s7/vendor/s7.{c,h}`，比照 lua 的 `vendor/dkjson.lua`）——`install-dev.sh`／`smoke.sh`／`build.sh` 都吃這份，零外部依賴。
- **⚠ glaze 反射的 struct 要放 namespace scope**：`llm_reflect.hpp` 的 `ask_as<T>`／`make_tool<Args>` 靠 glaze 靜態反射，函式內 local struct 編不過——struct 定義要放在 namespace／檔案層級。
- **未在 Windows 實測**（C 綁定的 `build.sh` 是 bash；Python/CL 改設 `LIBCLLM` 指 `libcllm.dll`）。
- **⚠ 動 C ABI 要想到這些下游**：改 `src/cabi.h` 扁平結構／`llm_ask` 簽章時，這 9 個綁定都在鐵律 3 的受影響清單裡。
