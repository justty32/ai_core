# galtxt try_4 — 三線整合（try_3 C++ 核心當底，s7＋Lua 薄層）

← [SESSION-LOG](../SESSION-LOG.md)｜[INDEX](../INDEX.md)｜姊妹線 [try_1（s7）](../try_1/README.md)・[try_2（Lua）](../try_2/README.md)・[try_3（純 C++）](../try_3/README.md)

**這條線把前三條合流**：try_1（s7 Scheme）、try_2（內嵌 Lua）、try_3（純 C++）各自證明了一塊，try_4 讓它們協作——

- **C++ 核心扛重活**：傳輸（WinHTTP／libcurl）、glaze 編譯期反射（struct＝唯一真相源）、schema 生成、`ask` 及三擴充（工具／多媒體／結構化輸出）。這一整塊**直接借 try_3 的核心**。
- **s7 與 Lua 只當薄薄一層**：吃 C++ 開出來的 function API，負責 REPL／同像性／改一行就跑的即時實驗手感。腳本不自己處理 JSON／HTTP／schema——那些留給 C++。

各取所長：C++ 的靜態反射＋原生效能，配腳本的動態靈活。

## ★ 核心思想：「用 try_3 當底」＝編譯層面，不是原地改 try_3

try_4 **不複製** try_3 的 `src/`，而是在自己的 [CMakeLists.txt](CMakeLists.txt) 裡把 try_3 的核心 `.cpp`**借編**進來：

```
add_executable(try4
    src/main.cpp                       # try_4 自己的殼
    ../try_3/src/http.cpp              # ↓ 借 try_3 的核心（除 main/cli/demo）
    ../try_3/src/llm.cpp
    ../try_3/src/llm_tool.cpp
    ../try_3/src/llm_media.cpp
    ../try_3/src/llm_json.cpp)
target_include_directories(try4 PRIVATE ../try_3/src)   # main 好 include 那些 header
```

為什麼這樣：

- **try_3 保持純基準線**：它「純原生、不嵌任何 VM」的身分是三線對照的價值所在，一字不改別變異掉。
- **核心不分叉**：改一次 `llm.cpp`，try_3 與 try_4 兩邊同時跟上（同一份檔），不會漂成兩套。
- **借編靠 `""` include 的搜尋規則**：`../try_3/src/llm.cpp` 內 `#include "http.hpp"` 以「含入檔所在目錄」為第一搜尋路徑，自動命中 `../try_3/src/http.hpp`，不必為核心之間的相依另設 include 路徑；只有 try_4 自己的 `main.cpp` 要 include 那些 header，才靠上面那行 `target_include_directories`。

try_4 只自備 try_3 沒有、或整合才需要的東西：新 `main`、s7／Lua 的嵌入初始化、把 C++ 函式綁成腳本 API、那兩層薄腳本。

## 現況（開發中）

**里程碑 1 ✓ 借編路徑已通（純 C++ 核心煙霧測試綠）**——`src/main.cpp` 用 try_4 自己的 main（非 try_3 的），include 借編的核心介面、呼叫 `llm::Client` 及三擴充，跑 try_3 的離線 fixture（`TRY4_TRY3_DIR` 由 CMake 注入指向 `../try_3`，連假回應都借、不另抄）。四種能力各驗一次全綠、中文無亂碼、`from_env()` 連結無誤。

**里程碑 2a ✓ 兩個 VM 已嵌入並起動（三世界共存煙霧綠）**——`try4.exe` 現把 **C++ 核心＋Lua VM＋s7 VM** 連進同一顆 exe，各自 eval 得動、中文乾淨（`[lua] 6*7=42、Lua 說：你好`／`[s7] (+ 1 2 3)=6、s7 說：你好`）。VM 來源已定＝**vendored 比照前線**，借編手法：

- **Lua 5.5**：借編 `../try_2/vendor/lua/*.c`，**只覆蓋它家改過的 `linit.c`**（try_2 在其中 preload 了 `cjson`/`http`；try_4 分工相反、JSON/HTTP 歸 C++，故換原味 linit＝[`src/lua_linit_clean.c`](src/lua_linit_clean.c)）。編成 static lib `lua_vendored`。
- **s7**：借外部 s7-playground 的 `s7.c`（同 try_1，不在本 repo；`S7_DIR` 指定，未給就試平台候選）。編成 static lib `s7_vendored`（`WITH_C_LOADER=0`、不定義 WITH_MAIN；MinGW 借 try_1 的 `shim_include` 補 `<sys/utsname.h>`，僅 Windows 僅此 lib）。
- **踩到的坑**：① `s7.h` 會 include `<complex.h>`（C++ 下有 `operator""i`），**不能包進 `extern "C"`**——Lua 標頭要包、s7.h 自帶守衛不可包，main.cpp 已分開。② SAC 這次光重連結不夠，得刪 `main.cpp.obj` 逼重編才放行。

**里程碑 2b ✓ 綁定層落地（非串流 ask 兩 VM 皆通、含錯誤路徑）**——try_4 現在是**雙 VM host**：`try4.exe scripts/demo.scm｜demo.lua` 依副檔名分派。腳本一句 `(llm-ask …)`／`llm.ask(…)` → C++ 綁定 → `bridge_ask`（唯一碰 `llm::Client` 的共用引擎）→ 借編核心 → file:// fixture → 答案回腳本，中文乾淨。錯誤路徑也實測：壞 endpoint → s7 `catch` 接到 `llm-error`、Lua `pcall` 回 false＋訊息，都不炸。

- **架構＝一個引擎、兩層薄綁定**：`bridge_ask`（[bind_bridge.cpp](src/bind_bridge.cpp)，`noexcept`）是唯一碰 `llm::Client` 之處；`s7_bind.cpp`／`lua_bind.cpp` 只把腳本字串轉一轉、呼叫它、把結果塞回各自 VM。加能力只改引擎，兩個綁定薄薄跟上。
- **⚠ 已解的關鍵坑＝C++ 例外 ↔ VM 的 longjmp**：`lua_error`／`s7_error` 都走 longjmp，會跳過還活著的非平凡 C++ 區域變數解構子。對策：① `bridge_ask` 設 `noexcept`，例外在踏進 VM 邊界前就收斂成 `bool＋err 字串`；② 綁定函式把 `std::string` 關進內層 scope，先把內容複製給 VM（Lua 堆疊／s7 前的 `char` 陣列），scope 結束解構後才呼 `*_error`。
- **命名坑**：`namespace bind` 撞 Windows `winsock.h` 的全域 `bind()`（main.cpp 有 include windows.h）→ 改名 `vmbind`。

**里程碑 2c ✓ 選項＋串流綁定（跨語言回呼實測全綠）**——`ask` 的完整表面都通了：
- **選項（table／`:keyword`）**：Lua `llm.ask{prompt=…, temperature=0.7, stream=true, on_delta=…}`；s7 `(llm-ask "…" :temperature 0.7 :stream #t :on-delta …)`（`s7_define_function_star`）。取樣欄位（model/temperature/top_p/max_tokens/seed）從腳本側傳入，`AskOpts`→`llm::Client` 只映射於 `bridge_ask` 一處；`optional` 有值才覆寫（對齊 try_3 哲學）。簡易形式 `llm.ask("你好" [, endpoint])` 仍可用。
- **串流回呼（最需試水的一段，已通）**：腳本函式包成 C++ `std::function<bool(std::string_view)>` 餵進 `ask`，在 `client.ask` 的 C++ 幀「之內」逐段回呼進 VM；回 true＝中止（對齊 try_3 極性）。**回呼的 longjmp 用 VM 保護呼叫關住**：Lua 走 `lua_pcall`、s7 走 `s7_call`（自帶 catch），腳本端錯誤不會 longjmp 穿過 C++ 幀。三條邊界路徑實測：逐段框印＋回完整答案、回 true 中途中止、回呼內 `error` 被接住轉成 VM 錯誤（Lua `pcall`／s7 `catch` 都捕得到、不炸）。
- 示範：[scripts/demo_stream.scm](scripts/demo_stream.scm)／[demo_stream.lua](scripts/demo_stream.lua)。⚠ 取樣數值（temperature 等）離線 fixture 不回顯，**是否真穿到請求得靠真後端驗**（比照 try_3 gotcha）。

**下一步（待接）＝三擴充**：工具呼叫／多媒體／結構化輸出比照綁上去（回傳型別較複雜：工具呼叫回 list、結構化回 struct，s7/Lua 側各要決定怎麼呈現）。

## 建置／執行

與 try_3 同一套 preset（[CMakePresets.json](CMakePresets.json)，`condition` 判平台，Windows 看 mingw、Linux 看 linux）：

```
cmake --preset mingw-debug          # 首次 configure（Linux 用 linux-debug）
cmake --build --preset mingw-debug  # → build/try4.exe
build/try4.exe                       # 跑核心煙霧測試
```

⚠ **Windows 新編 exe 偶被 SAC／Device Guard 擋**（`Permission denied`／被原則封鎖）——**重新連結一次即解**（雜湊變新就放行）：`rm -f build/try4.exe && cmake --build --preset mingw-debug`。已記 [common/gotchas](../workflows/common/gotchas.md)。

## 檔案

| 路徑 | 內容 |
|------|------|
| `CMakeLists.txt` | 借編 try_3 核心＋Lua/s7 各成 static lib＋編 try_4 的 main；glaze／winhttp(curl) 連結 |
| `CMakePresets.json` | mingw／linux 各 debug/release（`condition` 判平台，複製自 try_3）|
| `vcpkg.json` | 依賴 manifest（glaze）|
| `.clangd` | clangd 讀 `build/compile_commands.json`；濾掉 P1689 modules 旗標 |
| `src/main.cpp` | try_4 進入點；雙 VM host 分派（`.scm`→s7、`.lua`→Lua）＋無參數時的煙霧測試 |
| `src/bind.hpp` | 綁定介面：`bridge_ask`（共用引擎）＋`run_lua`／`run_s7`（各 VM host）|
| `src/bind_bridge.cpp` | `bridge_ask`：唯一碰 `llm::Client` 之處（`noexcept`，收斂例外不漏進 VM 的 C 幀）|
| `src/lua_bind.cpp` | Lua 綁定＋host：註冊 `llm.ask`、綁 `arg` 表、跑 `.lua`（含 longjmp 安全處理）|
| `src/s7_bind.cpp` | s7 綁定＋host：定義 `llm-ask`、綁 `*argv*`、跑 `.scm`（含 longjmp 安全處理）|
| `src/lua_linit_clean.c` | Lua 標準庫初始化的原味覆蓋版（取代 try_2 含 cjson/http preload 的 linit.c）|
| `scripts/demo.scm`／`demo.lua` | 薄層示範：一句 `llm-ask`／`llm.ask` 打離線 fixture（`*fixtures*`／`FIXTURES` 由 host 注入）|
| `scripts/demo_stream.scm`／`demo_stream.lua` | 串流示範：table／`:keyword` 帶 `on_delta` 回呼，逐段框印＋回完整答案 |
