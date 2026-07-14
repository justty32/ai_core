# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **try_1 玩具實驗場：LLM 接口跑通，現行主線＝s7 Lisp**（`sub_projs/galtxt/try_1/`，**刻意不套框架**；細節 [try_1/README](try_1/README.md)）。s7 版 `llm.scm`：`(llm-entry :prompt … :temp …)`，請求 inlet→`s7->json`、`(system "curl…" #t)` 打後端、回應 `json->s7` 導航；離線 curl `file://` E2E 綠。run：`cd try_1; <s7-playground>\s7.exe test.scm`。（C++ 版已放下留作參考。）
  - **✓ 已收（2026-07-14，上輪三待辦全清）**：① 抽出獨立 `json.scm`（llm.scm `(load)` 它）；② **schema 表生成簽章**——`*llm-schema*` 成唯一真相源，`make-llm-entry!` 用 `eval` 生成薄殼 `define*`、`llm-entry-impl` runtime 迭代 schema 塞取樣參數；手寫參數列消滅；三邊界驗過（新參數穿透／未知 keyword 被擋／缺 prompt 報錯）。③ **argv-aware host** `try_1/s7host.c`＋`shim_include/`：綁 `*argv*`（字串 list）＋load 腳本，`wmain`＋`-municode` 中文不亂碼；MinGW 上只需補 `<sys/utsname.h>` 一個 shim（不定義 `WITH_MAIN`、`WITH_C_LOADER` 自動 0）；實測 argv 傳遞／中文／exit code 全綠。build 指令與用法見 [try_1/README](try_1/README.md)「argv host」段。
  - **open 待辦（下一步）**：由 `*llm-schema*` **生成 `--flag` CLI 薄殼**（解析 `s7host.exe` 給的 `*argv*` → 呼叫 `llm-entry`），讓 `s7host.exe cli.scm --prompt "hi" --temp 0.7` 成真——這是「函式參數 ⇄ CLI 旗標由 schema 生成」洞見的最後一哩，schema／host 兩塊地基都已就位。
  - **待使用者**：接真後端（LM Studio 開 server / DeepSeek key）跑一次真回應——見 [WAIT_USER](WAIT_USER.md)。

- **try_2 玩具實驗場：C++ 內嵌 Lua 5.5 線，離線 E2E 全綠**（`sub_projs/galtxt/try_2/`，與 try_1 s7 線並存、兩條互為對照；細節 [try_2/README](try_2/README.md)）。地基：vendored Lua **5.5.0**（`vendor/lua/`＋`liblua.a`）＋ `host.cpp`→`host.exe`（內嵌 Lua、綁全域 `arg` 表、`-municode` 中文不亂碼、跑 .lua）。JSON＝**native C codec `native/cjson.c`**（★2026-07-14 已把 rxi/json.lua 手寫重寫成 C、編進 `liblua.a`、經 `linit.c` 一處增修註冊進 `package.preload`，`require("cjson")` 即得——host.exe 與 stock lua.exe 兩者皆內建、無 DLL、Windows/Linux 一致；語意對齊 rxi：序列自動陣列、中文原樣不 \u、null↔nil、`\uXXXX`＋surrogate pair 解碼、整值保 integer 型別）。HTTP＝**native C 傳輸 `native/http.c`**（★2026-07-14 已把 `io.popen("curl …")`＋暫存檔下沉成 C：Windows=WinHTTP（Schannel TLS、零安裝）／Linux=libcurl，同經 `linit.c` preload 註冊成 `require("http")`；請求 body 直接以字串進 C、零暫存、零外部 curl 依賴。分層：C 是笨管子只管 TLS＋round-trip／逐塊吐 bytes，SSE 拆框＋UTF-8 分批＋ask 縫全留 Lua。`file://` 特例由 http.c 自理保住離線 fixture harness，因 WinHTTP 不支援 file://。Windows 實測：離線 fixture 全綠＋真 WinHTTP GET/POST(UTF-8 body＋自訂標頭)/串流皆驗）。腳本層三塊：`llm.lua`（★schema 唯一真相源＋接口本體，`os.getenv` 讀 env、`require("cjson")`＋`require("http")`）、`cli.lua`（**由 schema 生成 `--flag` CLI**——try_1 還沒做到的那一哩，try_2 因 argv 從第一天就有而直接補上）、`test.lua`（離線）。schema 每列 `{名稱, JSON鍵或ctrl, 值型別}` 三欄，同時驅動 呼叫驗證／JSON對映／旗標名／值型別解析。離線全鏈路驗過：中文 prompt、取樣參數塞入、`--outfile`、型別/未知旗標/缺 prompt 全擋。另有 `lua.exe`（標準直譯器，同源編）＋`.vscode/`（Local Lua Debugger launch.json＋tasks.json＋sumneko settings）供 VSCode 除錯／建置。**已分層整理**：`src/`（llm/cli/_path）、`native/`（cjson.c＋http.c）、`examples/`（demo）、`test/`（test.lua＋`fixtures/`）、`build/`（產物, gitignored）；模組用 `debug.getinfo` 自解位址載入 sibling（location-independent、不綁 CWD），fixture 由 `src/_path.lua` 從專案根算出（不寫死機器路徑）；離線假回應改進版控當測試資料。
  - **★ 設計亮點：Lua 格式輸出（`--lua`/`lua=true`）**——讓 LLM 直接吐 **Lua table 字面量**，`load()` 成原生 table，跳過 JSON parse（內容層；API 信封仍 JSON）。同像性收束、多行中文台詞用 `[[…]]` 免跳脫。**沙箱**：`load(src,"=llm","t",{})`＝純文字＋空環境（無 os/io），杜絕 RCE；自動剝 ```` ```lua ```` 圍欄／補 `return`。離線示範 `demo_lua.lua`＋`fake_lua/` 假回應，原生 table 導航／運算＋沙箱擋 `os.exit` 皆綠。schema 也順勢長出 bool 旗標支援。
  - **★ API 縫定調（使用者拍板）：`llm.ask{…}` = Lua table 進、table 出**——JSON/HTTP 是縫背後可換實作（現 Lua、將來 C++ 原生函式推 table，呼叫端不改）。回傳選 (A)：只回模型答案 table，metadata 走旁路。`json.lua` 定位成「可退休的內部件」。
  - **★ 串流（`streaming=true`＋`callback`）**：即時回 `{ok=…}`、內容經 callback 持續餵；`chunk_chars` 設累積幾個 **UTF-8 字**才呼一次（Lua 5.5 `utf8` 庫，**絕不切半個中文字**、跨 SSE delta 重組）。傳輸＝`http.stream` 逐塊吐 raw bytes、Lua 端自拆 SSE 行（不靠網路分塊對齊行邊界）。CLI `--streaming` 用內建 stdout callback；`callback`（func 型）CLI 跳過。離線 `demo_stream.lua`＋`fake_stream/`，chunk_chars=3 精準分組＋拼回完整＋無半字皆綠。
  - **這幾個機制（Lua 格式輸出／ask 縫／串流）待使用者整體確認後可畢業去 llm_forge 固化。**
  - **踩坑收錄**（都記 [common/gotchas](workflows/common/gotchas.md)）：① SAC 偶發擋新編 exe 雜湊→重編即解；② Windows 中文兩坑——終端碼頁（host.exe `SetConsoleOutputCP` 自救）＋標準 `main` argv 是 950 碼頁（host.exe `wmain` 解決；lua.exe 中鏢，中文走 `--infile`）。
  - **跨平台（Windows ⇄ Manjaro 雙機開發）**：`build.sh` 用 `uname` 偵測（Windows 補 MinGW＋`-municode`；Linux 原生），產物名一律 `.exe`；Lua 端用 `package.config` 偵測 OS（`_path.lua` cwd 探測 cd/pwd、`llm.lua` 暫存目錄 TEMP//tmp）；`host.cpp` Windows 碼用 `#ifdef _WIN32` 包起、Linux 走純 main＋原生 UTF-8 argv。**Windows 已實測全綠；Linux 按跨平台寫法撰寫、尚未於 Manjaro 實測**（回家首跑要驗）。`.vscode/` 僅 Windows VSCode 用；Manjaro 改用 **neovim/lazyvim**（除錯設定待該環境自建，暫不處理）。
  - **工具路標（評估過、暫不接，免下次重查）**：CLI 現手刻 `cli.lua`（從 schema 生成）最貼合；**長出子命令**時選 `mpeterv/argparse`（單檔純 Lua、MIT、最主流，優於 lua_cliargs／lummander），且仍**從 `llm.schema` 生成**其定義。REPL＝stock `lua.exe -i`（已是 5.5，不需 LuaConsole＝停在 5.4）；要行歷史再給 lua.exe 編 **linenoise**。串流想上色＝`ansicolors`/`lua-term`。shell 膠水 `luash` 不適用（`os.execute` 不能增量串流）。
  - **★ 定位拍板（2026-07-14）：try_2 ＝ 純 Lua，Fennel 放棄**（使用者：想玩 Lisp 走 try_1 s7 線；不上 Fennel）。曾規劃 AOT＋host 認 `.fnl`＋設定系統，**未動任何 code 即撤**（含快取撞名的解法：cache 檔名綁源檔絕對路徑 hash——備忘，日後若別處要編譯快取可重用）。
  - **open 待辦（下一步）**：核心已到**穩定歇點**——schema 接口＋native C `cjson`/`http`＋串流＋Lua 格式輸出＋schema 生成 CLI 全綠。剩：① 真後端實測、② Manjaro 跨平台驗（皆見 [WAIT_USER](WAIT_USER.md)）。**遠程**：把 try_1(s7)／try_2(Lua)／try_3(純 C++) 三線整合（方向待定）。可選深化：讓 C 直接推原生 table（`lua_newtable`）、省掉 Lua 端 `cjson.decode` 那一跳——等瓶頸或整合需求再說。
  - **待使用者**：接真後端跑一次真回應（與 try_1 共用那關）——見 [WAIT_USER](WAIT_USER.md)。

- **try_3 玩具實驗場：純 C++／C++20 modules 線，除錯骨架就緒**（`sub_projs/galtxt/try_3/`，與 try_1/try_2 並存、三線互為對照；細節 [try_3/README](try_3/README.md)）。**完全 C++、不嵌腳本 VM**，並以 **C++20 named modules** 當骨架示範。建置走 **CMake + Ninja + vcpkg toolchain**（釘死 MinGW `C:/dev/mingw64` g++16、ninja＝vcpkg fetch 的 1.13.2、toolchain＝`C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake`）——與 try_2 手寫 `build.sh` 走不同路、對照。結構：`src/demo.cppm`（具名模組 `demo`，export `sum_to`/`greet`）＋`src/main.cpp`（`import demo;`、wmain/console）；`CMakeLists` 用 `target_sources(... FILE_SET CXX_MODULES ...)`。完整 `.vscode/`（cppdbg＋gdb 的 `launch.json`、cmake `tasks.json`、`settings.json`、`c_cpp_properties.json`）。已實測全綠：Ninja 掃 module 相依→生 dyndep→先編 BMI 再編 main→連結；`try3.exe` 正確中文與計算；gdb batch 驗**進模組內** `sum_to@demo`（`break demo.cppm:16` 命中、`info args`、單步、堆疊回溯 `sum_to@demo→run→wmain→CRT`、`std::vector` pretty-print）。**Windows 已驗；Linux/Manjaro 需改 preset/`.vscode` 工具鏈路徑（README 有記）。**
  - **★ modules 關鍵**：generator **必須 Ninja/VS**——MinGW Makefiles 不支援 module 掃描（CMake 直接報錯，這是換掉 Makefiles 的原因）。cmake 需 3.28+、ninja 需 1.11+。
  - **設計要點**：`-static` 靜態連 libstdc++/libgcc/winpthread → 獨立 exe，不靠 PATH（否則 gdb 啟 exe 找不到 runtime DLL）；中文靠 `wmain`+`-municode`+`SetConsoleOutputCP`（同 try_2）；`#ifdef _WIN32` 包 Windows 碼、Linux 走純 main。
  - **踩坑**（併記 [common/gotchas](workflows/common/gotchas.md)）：① SAC 偶發擋新編 exe（重連結產生新 hash 即解，同 try_2）；② 從 Git Bash 直呼原生 `gdb.exe`，多字詞 `-ex "break foo"` 引號分組會遺失→改 `gdb --batch -x cmds.gdb`（VSCode cppdbg 走 MI 不受影響）；③ **C++20 模組 export 函式帶模組修飾**（gdb 顯示 `sum_to@demo`）——編輯器行號槽中斷點（file:line）正常，但按函式名下中斷要 `break sum_to@demo` 或 `break demo.cppm:16`。
  - **★ vcpkg 依賴鏈已驗（2026-07-14）**：接第一個庫 **glaze 7.8.4**（反射式 JSON↔struct、header-only、超快）＝兼作整條 vcpkg→MinGW→連結驗證。manifest 模式（`vcpkg.json`）；**MinGW triplet 雷**：預設 `x64-windows`（MSVC）會壞，preset 設 `VCPKG_TARGET_TRIPLET`/`VCPKG_HOST_TRIPLET`＝**`x64-mingw-static`**（與本線 `-static` 一致）；CMake `find_package(glaze CONFIG REQUIRED)`＋link `glaze::glaze`；glaze 需 **C++23**（`CMAKE_CXX_STANDARD` 提到 23、g++16 支援）。`main.cpp` `demo_json()` 實跑：struct→JSON／JSON→struct 雙向對、中文原樣 UTF-8、exe 仍全靜態獨立。裝到 `build/vcpkg_installed/`（gitignored）。加庫套路＝`vcpkg.json` 加名＋`find_package`＋link＋重配置。（用 glaze＝取代不了的原因：try_2 `cjson.c` 是 JSON↔Lua value、綁 Lua C API，try_3 無 Lua 拿不來，且它不做 struct 映射。）
  - **open 待辦（下一步）**：骨架＋glaze 就緒，開始長真東西（方向待定；此線定位＝純原生 C++／modules 對照組）。

> 註：舒適 CL 環境已另立為兄弟子專案 [`sub_projs/comfy/`](../comfy/README.md)（與本 s7 線並存、兩邊都推），其活狀態記在該處與頂層 [SESSION-LOG](../../SESSION-LOG.md)，不在 galtxt hub 裡。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
