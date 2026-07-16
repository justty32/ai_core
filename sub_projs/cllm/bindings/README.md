# bindings — 各語言用 cllm（含直接用 C ABI 的 C/C++）

← [cllm 專案根](../README.md)｜[C ABI 參考](../docs/c-abi-reference.md)｜[INDEX](../INDEX.md)

把 `libcllm.so` 的**對外 C ABI**（唯一入口 `llm_ask`）綁進各語言，一句話問 LLM。**只消費穩定 C ABI、不碰 cllm 的 CMake/vcpkg**（刻意輕）。API 對齊 galtxt/try_4：`ask` + `on_delta` 串流回呼，回完整答案字串。

## 常駐開發環境（推薦入口）

**[`../install-dev.sh`](../install-dev.sh)** 一鍵把 cllm 裝成常駐前綴（預設 `~/repo/dev`，像 `/usr/{include,lib,bin}`）＋搭好全部語言環境，**可重現**（`rm -rf ~/repo/dev` 後重跑即重建）：

```bash
bash install-dev.sh            # → ~/repo/dev/{include,lib,bin,share}
source ~/repo/dev/env.sh       # 設好 PATH／pkg-config／lua cpath／LIBCLLM／CLLM_LISP…
# 逐語言範例（離線 fixture，$CLLM_FIXTURES 由 env.sh 設）：
bash   ~/repo/dev/share/cllm/examples/shell/example.sh   "$CLLM_FIXTURES"
python3 ~/repo/dev/share/cllm/examples/python/example.py "$CLLM_FIXTURES"
```

各語言用法＋JSON 庫對照見 `~/repo/dev/share/cllm/README.md`（由腳本生成）。

## 各語言（原始碼在此，install-dev.sh 建置/複製）

| 語言 | 產物／用法 | JSON 庫 | 原始碼 |
|------|-----------|---------|--------|
| C    | `cc x.c $(pkg-config --cflags --libs cllm jansson)`；`<cllm/cabi.h>` | jansson | [c/](c/) |
| C++  | `g++ -std=c++20 …`；`<cllm/cabi.hpp>`（`llm::abi`）| jansson | [cpp/](cpp/) |
| Lua  | `require("llm")`（5.5＝`lua`、5.4＝`lua5.4`）| dkjson | [lua/](lua/) |
| Fennel | `(require :llm)`（跑在 lua 5.4）| dkjson | [fennel/](fennel/) |
| s7   | `llm-s7 script.scm`（`llm-ask` 內建）| jq（shell-out）| [s7/](s7/) |
| Python | `import llm`（純 ctypes、零編譯）| stdlib `json` | [python/](python/) |
| Common Lisp | `(load CLLM_LISP)`→`cllm:ask`（CFFI）| shasht（quicklisp）| [lisp/](lisp/) |
| Go | `import "cllm"`（cgo，pkg-config cllm）| stdlib `encoding/json` | [go/](go/) |
| Shell | `llm 你好`（unix filter CLI）| jq | [shell/](shell/) |

每個 example 都示範：基本 `ask`、串流、`schema`→JSON 解析、以及從該語言 shell-out 呼叫 `llm` CLI。

## 映像/部署（Lisp 家族：s7／Common Lisp／Fennel）

把綁定＋你的碼做成**映像**（image）＝快啟動、可當 CLI/lib、還能執行期改。各語言能力不同，見各自 `image/`：

| | 記憶體映像 dump | 產獨立執行檔 | 執行期修改 | 改動存回映像 | 細節 |
|---|---|---|---|---|---|
| **Common Lisp (SBCL)** | ✅ `save-lisp-and-die` | ✅ exe/core | ✅ | ✅ | [lisp/image/](lisp/image/README.md) |
| **s7** | ❌ | ✅ 烤進 C host | ✅ | ❌（重編）| [s7/image/](s7/image/README.md) |
| **Fennel/Lua** | ❌ | ✅ AOT `.lua` | ✅ | ❌（重編）| [fennel/image/](fennel/image/README.md) |

## 選項（三種寫法，語意一致）

- **Lua / Python**：`ask(prompt[, endpoint], opts…)`，opts 用 **snake_case**（`on_delta`／`api_key`／`max_tokens`）。
- **s7 / CL**：`(llm-ask prompt [endpoint] :key val …)`／`(cllm:ask prompt [endpoint] :key val …)`，**hyphen keyword**（`:on-delta`／`:api-key`）。
- **Go**：`Ask(prompt, opts...) (string, error)`，functional options（`cllm.Endpoint(url)`／`cllm.Temperature(0.7)`／`cllm.Stream()`／`cllm.OnDelta(fn)`）；錯誤走回傳的 `error`。
- 欄位：`endpoint`／`api_key`／`model`／`timeout_ms`／`temperature`／`top_p`／`presence_penalty`／`frequency_penalty`／`max_tokens`／`seed`／`stream`／`schema`／`on_delta`／`on_error`（皆選填）。回完整答案字串。

## 注意（v1，刻意小）

- **⚠ Fennel 用 lua 的鍵名**：fennel 呼叫的是 lua binding，table 鍵要用**底線** `:on_delta`（不是 fennel 慣用的 `:on-delta`——那會變成 `"on-delta"` 對不上）。
- **⚠ Lua 模組 ABI 綁 lua 版本**：`lua`（5.5）與 `fennel`（5.4）各需一份 `llm.so`（install-dev.sh 兩版都編，env.sh 用 `LUA_CPATH_5_4/5_5` 分流）。
- **⚠ s7 回呼別丟 error**：`:on-delta`/`:on-error` 在 `llm_ask`（C++）途中被呼叫，s7 error 以 longjmp 實作、穿 C++ 是 UB。回呼只做無害的事。（Lua/Python/CL 有包 pcall/handler-case，安全。）
- **Common Lisp 需 quicklisp**（載 CFFI／shasht）；`~/.sbclrc` 已載 quicklisp 即可，`--script` 下 cllm.lisp 會自載 `~/quicklisp/setup.lisp`。
- **Go 走 cgo**：`#cgo pkg-config: cllm` 於 `go build` 時跑 pkg-config，故需先 `source env.sh`（設 `PKG_CONFIG_PATH`）。回呼用 `cgo.Handle` 把 Go closure 帶進 C，schema struct 用 `runtime.Pinner` pin（Go 1.21+ 內嵌 Go 指標規則）。
- **只綁 text／schema／stream 主路**：`tools`／`media`／`modalities` 未收（C ABI 已支援，補接即可，見 [C ABI 輸入型](../docs/c-abi-input.md)）。
- **未在 Windows 實測**（C 綁定的 `build.sh` 是 bash；Python/CL 改設 `LIBCLLM` 指 `libcllm.dll`）。
- **⚠ 動 C ABI 要想到這些下游**：改 `src/cabi.h` 扁平結構／`llm_ask` 簽章時，這 8 個綁定都在鐵律 3 的受影響清單裡。
