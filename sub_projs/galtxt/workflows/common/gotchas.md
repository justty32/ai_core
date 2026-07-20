# 共通踩坑（跨工作流）

← [INDEX](../../INDEX.md)

不專屬任一工作流、任何人都可能撞到的坑，記/查這裡。某工作流專屬的坑記在該工作流自己的 `gotchas.md`（長出來後在下表加一列導流）。

## 哪類坑記哪裡

| 坑的性質 | 記/查這裡 |
|---------|----------|
| 不專屬任一工作流的共通坑 | **common/gotchas**（本檔）|

條目格式：一條一個坑，**粗體標題 + 一兩句現象與對策**，必要時連結細節。

---

- **Windows 中文：終端碼頁 vs 命令列參數，兩個獨立的坑**。① **輸出亂碼**＝終端碼頁非 UTF-8（繁中預設常 950），程式吐 UTF-8 被讀錯——對策：exe 啟動 `SetConsoleOutputCP(CP_UTF8)` 自救（try_2 host.exe 這樣做），或終端 `chcp 65001` / PowerShell `[Console]::OutputEncoding`。② **中文命令列參數壞**＝MinGW 標準 `main(argv)` 拿到的是系統碼頁（950）不是 UTF-8——對策：用 `wmain`＋`-municode`＋`WideCharToMultiByte` 轉 UTF-8（try_2 host.exe、try_1 s7host.exe 都這樣），或把中文走檔案（`--infile` 讀 UTF-8）而非 argv。標準 `lua.exe`（無 wmain）就中鏢②，所以它只適合 debug、餵中文靠 `--infile`。
- **Smart App Control（SAC）偶發擋掉剛編好的新 exe**：本機（Windows 11）SAC 為 On，會把**某顆新編、沒信譽的 exe 雜湊**判成 unknown→封鎖，跑時報「應用程式控制原則已封鎖此檔案」。**這不是程式錯誤**——關鍵是讓 exe 的**雜湊變成一個 SAC 願意放行的新值**。`Unblock-File`（MOTW）無關、無效。⚠ **2026-07-15 修正一個先前的誤解**：以為「重編一次／換輸出檔名」即可，但 GCC 連結是**決定性的**——原始碼沒變，重連結（甚至刪 `.o` 重編）會**重現同一顆被擋的雜湊**；`cp` 成新檔名也是同位元組同雜湊，照擋。真正可靠：**改變二進位內容**。實測 `strip build/xxx.exe`（去符號→體積與位元組全變→新雜湊）一次放行；或真的改一行原始碼再編。（try_2 host.exe、try_4 try4.exe 皆踩過；try_4 開發迴圈直接對 stripped 副本跑。）
- **Git Bash 跑剛連結好的 exe 報 `Permission denied`（exit 126），但 PowerShell 跑同一支正常**：這**不是 SAC**（SAC 會兩個殼都擋），是 Git Bash 對「剛被 `ar`/`gcc` 寫出、exec 位元狀態未穩」的 exe 偶發拒跑。**別誤診成 SAC 去重編**——直接改用 PowerShell（或 cmd）跑，或 `chmod +x`。判別法：PowerShell 跑得動＝非 SAC＝Bash 的 exec-bit 坑；PowerShell 也被擋＝才是 SAC，重編。
- **glaze 反射 JSON 預設遇未知鍵報錯**：`glz::read_json(obj, src)` 用預設選項，只要 `src` 有 `obj` struct 沒有的欄位就整包解碼失敗。解真後端回應（OpenAI chat completion 這種欄位一大堆、你只挑 `choices[0].message.content` 幾個）必踩。對策：`glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)`——忽略多餘鍵、只填 struct 有的。（cllm（原 try_3）`archived/main.cpp demo_http()` 踩過，見 [cllm/README](../../../cllm/core/README.md)。）
- **匿名 namespace 的同名 struct 讓 glaze COMDAT section 跨 TU 撞名（MinGW/GCC）**：多個 `.cpp` 各自在**匿名 namespace** 定義同名 struct（如 `ReqBody`），GCC 把匿名 namespace **一律 mangle 成 `_GLOBAL__N_1`**（不保證每 TU 唯一），於是 glaze 為每個型別生成的 `quoted_key_v` 之類 `static constexpr` 走 COMDAT/weak section，section 名含 `_GLOBAL__N_1::ReqBody` → 跨 TU **撞名**。若兩個 `ReqBody` 欄位不同，連結器報 `duplicate section … has different size`，折疊後**可能給錯 JSON 鍵**（不只是警告，是潛在序列化 bug）。對策：**別用匿名 namespace，改各 TU 唯一命名的內部 namespace**（如 `tool_impl`/`media_impl`/`json_impl`）＋ `using namespace X;`。（cllm（原 try_3）`archived/llm_*.cpp` 踩過，見 [cllm/README](../../../cllm/core/README.md)。）

### 編輯器整合（2026-07-14 接 Neovim/LazyVim 時收；VSCode 側多半同理）

- **★★ Lua 5.5 把 generic `for` 的控制變數改成唯讀（const）——還沒跟上的 Lua 庫會在 5.5 上「編譯期」直接死**：`for x in … do x = f(x) …` 這種「拿控制變數當可寫暫存」的老寫法，在 5.5 是 `attempt to assign to const variable 'x'`，而且是**編譯期**錯誤——整個檔案載入不了，不是執行到那行才炸。**症狀極度誤導**：以 `local-lua-debugger-vscode`（0.3.3）為例，它注入被除錯行程的 `lldebugger.lua` 就中這招，於是 DAP session 起得來、中斷點還顯示 `verified=true`，然後程式直接跑完結束、什麼都沒停——看起來像「中斷點沒作用」，其實是除錯器本體根本沒載入。要看到真正的錯誤得去讀 DAP 的 `output/stderr` 事件。修法就是換個變數名（`for raw in … do local x = f(raw)`）。**凡是要在 Lua 5.5 上跑第三方 Lua 庫（尤其是從 TypeScript/Fennel 之類編譯出來的），都要留意這個模式。**
  **galtxt 的現況（使用者拍板 2026-07-14）**：**Lua 除錯一律走 Neovim**——修補做成 nvim 設定裡的**啟動時冪等 patch**（檔案在 mason 套件目錄下，更新會被覆蓋回去；偵測到壞的樣子才改，上游修好就是 no-op）。**VSCode 側已知不可用、刻意不修**（沒地方掛啟動 hook）；真要修就手動改 `~/.vscode/extensions/tomblind.local-lua-debugger-vscode-*/debugger/lldebugger.lua` 的那一行，但擴充更新會覆蓋回去。詳見 try_2 的 README「除錯 Lua」（三線已移除，內容見 git log）。
- **★★ `verified = true` **不代表**中斷點會停——它只代表「adapter 收到了」**：DAP 的 `setBreakpoints` 回應裡那個 `verified`，各家除錯器的認真程度差很多。`local-lua-debugger-vscode` 是**照單全收**：你把中斷點下在**空行、註解行、`end`**，它一樣回 `verified = true`，然後**永遠不會停**——它不會像多數除錯器那樣把中斷點自動往下貼到下一個可執行行。於是症狀又是那個熟悉的「看起來一切正常、就是不停」。
  **實測定樁**（try_2 `test/test.lua`，同一組設定只差一行）：下在 `io.write(llm.last_request .. "\n")` → `stopped reason="breakpoint"`；下在它**下一行的空行** → 跑完全程、從不停。
  ⚠ **這條特別容易在「改過檔案之後」復發**：昨天驗證過「第 20 行會停」，你今天在上面插了幾行，第 20 行就變成空行了——**行號不是穩定的座標**。中斷點沒停時，第一件事是回去看那一行到底是不是敘述。
  同源陷阱：拿「跑目前開啟的檔案」這種設定去除錯**模組**（只 `return M`，函式體一行都不跑），中斷點也是 `verified=true` 卻永遠不停——要停在模組內部，得從真的會呼叫進去的主程式啟動。
- **codelldb 只監聽 `127.0.0.1`（純 IPv4）——adapter 寫 `host = "localhost"` 會在 IPv6 優先的機器上 ECONNREFUSED**：`getaddrinfo("localhost")` 先回 `::1` 時，nvim-dap 連過去就被拒。**LazyVim 自己的 clangd extra 就是寫 `host = "localhost"`**（在多數機器上碰巧沒事，因為 getaddrinfo 先回 IPv4），照抄它就會踩到。對策：**別給 `host`**（nvim-dap 預設 `127.0.0.1`），或明確寫 `127.0.0.1`。
- **註冊某個 DAP adapter 名字，會招來 mason 自動下載對應套件**：`mason-nvim-dap` 的 `automatic_installation` 掃的是 `dap.adapters` 的 **key**，看到 `cppdbg` 就去解析成 `cpptools`（微軟那包 ~90MB 的 VSCode 擴充）然後開始下載——即使你只是想把 `cppdbg` 這個名字接到別的除錯器。對策：`automatic_installation = { exclude = { "cppdbg" } }`。反過來，`setup_handlers` **只對已安裝的套件動作**，所以套件沒裝就不會有 handler 跑來覆蓋你自己註冊的 adapter。
- **CMake 3.28+ 配 Ninja 會塞 C++20 modules 的掃描旗標，clangd 看不懂**：即使專案完全不用 modules（一行 `import`／`export` 都沒有），Ninja generator 仍會給每個編譯單元加 `-fmodules-ts`／`-fmodule-mapper=…`／`-fdeps-format=p1689r5`（P1689 依賴掃描）。這些旗標會原樣進 `compile_commands.json`，clang 驅動器不認得 → 每個檔都噴 `Unknown argument` 診斷雜訊。對策：`.clangd` 的 `CompileFlags.Remove` 過濾掉（try_3 有實例）。任何吃 `compile_commands.json` 的工具（clang-tidy 等）都可能中同一招。
- **clangd 不會自己下探 `build/` 找 `compile_commands.json`**：它只從當前檔案往**上層**找。CMake 產在 `build/` 底下，所以要在專案根放 `.clangd` 寫 `CompileFlags.CompilationDatabase: build`。手寫建置（try_2 的 `build.sh`）則直接把 CDB 寫到專案根。這兩種做法都是**編輯器中立**的（nvim 與 VSCode 的 clangd 都讀），比在編輯器設定裡塞 `--compile-commands-dir` 好。
- **★★ 把 REPL 接進編輯器（stdio 型）時，REPL 的 prompt 會卡在 libc 的全緩衝裡出不來——「起得來但永遠不回話」**：Conjure 這類 stdio client 是靠**認得 prompt 字串**來判斷「這次求值回完了」。而 REPL 的 prompt 走 **stdout**；stdout 一旦不是終端機而是 pipe，libc 預設就從行緩衝變成**全緩衝（4KB）**——`> ` 才 2 bytes，永遠填不滿，於是 prompt 卡在緩衝區裡。client 等不到 prompt 就一直認為「還沒回完」，**結果一行都不會出現**。症狀很安靜：log 顯示 REPL `(started)`、eval 也確實送出去了，就是沒有結果。（s7 的 banner 走 **stderr**、stderr 無緩衝，所以你偏偏看得到 banner，更容易誤判成「REPL 活著、是求值壞了」。）
  **⚠ 這個 bug 有個會騙人的「驗證法」**：在 shell 裡 `printf '(+ 1 2)' | s7` 看得到 prompt 和結果——但那是因為 s7 讀到 **EOF 就結束、退出時 libc 會 flush**。**行程不退出的情境（正是編輯器接 REPL 的情境）它完全不成立**，所以這個測法無法暴露 bug，別拿它當「REPL 沒問題」的證據。要測就得**讓 stdin 保持開著**（fifo）再看幾秒內 stdout 有沒有東西。
  對策：把 REPL 包一層 **`stdbuf -o0`**（LD_PRELOAD 改掉 stdout 緩衝模式）。必須是 `-o0`（無緩衝）**不能**是 `-oL`（行緩衝）——prompt 結尾沒有換行，行緩衝一樣不會 flush。**凡是 prompt 不以換行結尾的 REPL 都吃這招。**（`~/repo/my_lazyvim_settings/lua/plugins/scheme.lua` 的 s7 踩過。）
  順帶：Conjure 的 `#stdio#command` 要給**字串**不能給 table——它會直接字串串接去顯示狀態、也會對它做 `string.match` 判方言，給 table 兩邊都炸；代價是路徑不能含空白。

### Linux／跨平台（2026-07-14 Manjaro 首跑收）

- **★ Lua 在 POSIX 上不給 `-DLUA_USE_LINUX`，`io.popen` 會是「不支援」的錯誤樁**：`luaconf.h` 只在看到 `_WIN32` 時**自動**定義 `LUA_USE_WINDOWS`；POSIX 這邊**沒有對應的自動偵測**，你不給 `-DLUA_USE_LINUX`（或 `-DLUA_USE_POSIX`）就沒有 `LUA_USE_POSIX`，於是 `liolib.c` 的 `l_popen` 編成 `luaL_error(L, "'popen' not supported")` 的樁、`os.tmpname` 退回 `tmpnam`（編譯還噴警告）。**症狀很誤導**：同一份 Lua 碼在 Windows 好好的、Linux 上 `io.popen` 直接報錯，看起來像是 Lua 碼的跨平台 bug，其實是**建置旗標**漏了。`LUA_USE_LINUX` 連帶開 `LUA_USE_DLOPEN`，只需再補 `-ldl`——**不需要** `-lreadline`（readline 是 `lua.c` 執行期 dlopen `LUA_READLINELIB`，找不到只警告）。⚠ **別用「改讀 `$PWD`／`$CD` 環境變數」繞過** `io.popen`：cmd/PowerShell **不把 `CD` 傳進子行程的環境區塊**，那樣會在 Windows 上靜靜退化成 `"."`——治標還反向弄壞另一個平台。（try_2 `build.sh` 的 `LUAPLAT` 踩過。）
- **★ `shim_include/` 只給 MinGW，Linux 絕不可放進 include path**：`try_1/shim_include/sys/utsname.h` 是補 MinGW **缺**的標頭；Linux 原生**有**真正的 `<sys/utsname.h>`，一旦誤把 `-I ./shim_include` 塞進 Linux build，會**蓋掉系統標頭**、換上假資料版 `uname()`。同理任何「補某平台缺件」的 shim 目錄都該只掛在該平台分支。（try_1 `build.sh` 的 `EXTRA_INCLUDES` 只在 MinGW 分支給。）
- **s7 的 `WITH_C_LOADER` 兩平台預設相反，連結旗標不能共用**：Linux 上有 `dlfcn.h`／`sys/utsname.h`，`WITH_C_LOADER` **預設開**（FFI／現編 `*_s7.so`），要 `-ldl` ＋ `-Wl,-export-dynamic`（讓現編的 .so 連得回 s7 符號）才連得過；MinGW 上這開關**自動變 0**，那兩個旗標反而不該給。所以 s7 的 build 一定要分平台，不存在一組通用旗標。（try_1 `build.sh`。）
- **s7 沒有跨平台 `getcwd`（Scheme 層拿不到工作目錄）**：要組本機絕對路徑（例如離線 fixture 的 `file://` URL）只能借道 `(system "pwd" #t)`——記得**砍掉輸出尾端的換行**。副作用是腳本**綁 cwd**（一定要在該目錄下跑）。（try_1 `test.scm`。）
- **vcpkg toolchain 不會攔截系統的 `find_package(CURL)`**：怕 vcpkg 搶走而特地在 `vcpkg.json` 加 `curl`（→ 從源碼編、很慢）是多餘的。vcpkg toolchain 只是在標準 CMake 模組搜尋**之前**插入自己的搜尋路徑，沒裝就自然落回系統庫。Manjaro 上 `find_package(CURL REQUIRED)` 直接命中 `/usr/lib/libcurl.so`。（try_3。）
- **vcpkg manifest 沒釘 `builtin-baseline`，兩台機器會解出不同版本**：同一份 `vcpkg.json` 因各機 registry 快照不同而版本漂移（實例：try_3 的 glaze，Windows 7.8.4 / Linux 7.4.0）。這次 API 沒變、行為一致，但**跨機潛在地雷**——要鎖版就加 `builtin-baseline` 或 `version>=`。
### 真後端（2026-07-14 三線首次打通 LM Studio 收）

- **★★★ 方法論：「自己寫的離線 fixture」結構上就測不出「真後端長什麼樣」**。這不是抽象警告——**同一個病根已經連咬三次**：
  ① **try_3 的 `required`**：假回應是我們自己寫的、欄位當然齊全，所以「模型會少吐欄位」這種缺陷**永遠**測不到。
  ② **try_1 的 `json.scm` 跳脫引號**：假回應剛好沒有 `\"`，所以「只認得一個跳脫引號」的 parser bug 躲過了整套離線測試，一打真後端（reasoning 模型的思考鏈裡一堆 `\"`）就整包解不出來。
  ③ **try_4／try_3 核心的「後端 error JSON 被靜默吞成空字串」**（2026-07-15）：離線 fixture **從不回錯**，於是「後端出錯時 `ask` 回空字串卻報成功」這個護欄破洞躲過整套離線測試；一打真後端（LM Studio 模型 JIT 載入 SIGABRT → HTTP 400＋`{"error":{"message":…}}`、**無 choices**）就現形——`ask` 回 `""`、`pcall ok=true`，呼叫端分不清「模型回空」與「後端出錯」。見下方獨立條目。
  **共同結構**：fixture 是「我們以為後端會回什麼」的投影，於是它只能驗「我們已經想到的東西」——尤其是**它只投影「成功」、從不投影「失敗」**。**對策**：離線 fixture 該**刻意塞進真實世界的髒東西**（跳脫序列、超長字串、缺欄位、多餘欄位、reasoning_content 之類的意外欄位，**以及 error JSON／4xx 這種失敗回應**），而不是寫一份乾淨漂亮的樣本；並且**新接口一定要對真後端打一次**才算數。（try_1 已補上跳脫序列的定樁測試。）
- **s7 的 `json.scm` 字串掃描曾只認得「一個」跳脫引號**：原版邏輯是「找下一個 `\"`；若它前一字元是 `\` 就再找下一個」——字串裡有兩個以上的 `\"` 就提早跳出、把內文當 JSON 語法解析，吐 `bad entry`、整包回應解不出（`llm-entry` 回 `#f`）。已改成逐字掃描、正確處理反斜線，並把 JSON 跳脫序列（`\"` `\\` `\/` `\n` `\t` `\r` `\b` `\f` `\uXXXX`＋surrogate pair）**解碼成真正的字元**、再用 s7 的 `write` 印成合法 Scheme 字面量（escape 正確性交給 s7，不手工拼接）。⚠ 原版還有個潛在雷：`\uXXXX` 直接照抄進 Scheme 字面量，**s7 根本讀不懂**。

- **★★ 後端 error JSON 別靜默吞成空字串——`choices` 空就回 `{}` 是護欄破洞**（2026-07-15，try_4 真後端驗揪出、修在共用核心 try_3）：OpenAI 相容後端出錯（LM Studio 模型載入失敗、雲端 401/429…）回的是 `{"error":{"message":…}}`＋HTTP 4xx，其回應**沒有 `choices`**。三線核心原本都寫「解不動或 `choices` 空 → `return {}`」（`llm.cpp` 非串流、`llm_json/llm_tool/llm_media` 四個非串流入口，加串流路徑），於是**後端錯誤被吞成空字串、還報成功**——對「笨模型＋護欄」北極星，這正是最該堵的洞（呼叫端拿到 `""` 卻以為模型只是沒話說）。**對策**：解析前先攔錯——抽一個 `throw_if_backend_error(status, raw)`（解 `{"error":{"message":…}}` 有就 throw、或 HTTP≥400 帶狀態碼＋body 片段 throw），**塞在共用的 `post()` 一處**→ 四個非串流入口全數跟上；串流路徑另接 `http::stream` 回傳的 status＋累積的 raw 再攔一次。noexcept 的綁定橋會把例外收斂成 VM 錯誤（Lua `pcall` 得 false＋訊息、s7 `catch` 得 `llm-error`）。**離線 file:// fixture 回 status=200 且無 error 物件，不受影響**（實測 10 支示範不回歸）。★ 這也印證前面方法論條目 ③：**fixture 只投影成功、從不投影失敗**，錯誤路徑非打真後端不可。
- **★ reasoning 模型：`max_tokens` 是「reasoning＋content」一起算的——手動給小值會拿到空答案**：gemma-4-e4b 這類 reasoning 模型把思考鏈放在 `message.reasoning_content`、真正答案才在 `message.content`，**兩者共用同一個 `max_tokens` 預算**。實測手動給 `max_tokens=600`（對「一句台詞」看起來綽綽有餘）→ 574 個 token 全被 reasoning 吃光、`finish_reason: length`、`content` 只剩空字串或半截 JSON。三條線都只抽 `choices[0].message.content`，症狀就是「後端明明有回、程式卻拿到空的」，**極易誤診成自己的解析壞掉**——看一眼 `usage.completion_tokens_details.reasoning_tokens` 就知道預算花去哪了。
  **⚠ 別過度反應：不設 `max_tokens` 的預設路徑本來就是對的。** 三線的取樣參數（含 `max_tokens`）**全是選填、沒設就不送**，交後端用它自己的默認（LM Studio＝該模型的 context 上限，本機設 71936）——實測不送 `max_tokens` 時 `finish_reason: stop`、reasoning 花掉 546 token 之後 content 照樣完整生出來。所以這坑**只在你手動設了 `max_tokens` 時才咬人**：要嘛別設（讓後端自己來），要設就給 reasoning 留足空間。「使用者沒設的就別替後端做主」這條原設計在這裡救了自己一命。
- **★★ `glz::write_json_schema<T>()` 用預設 opts 不生 `required` → 模型可以少給欄位**：glaze 生成的 schema 有 `properties` 和 `additionalProperties:false`，但**沒有 `required` 陣列**。JSON Schema 語意下沒列進 `required` 的欄位＝**選填**，於是後端的約束解碼只保證「不多給鍵」、**不保證「每個欄位都給」**——實測 LM Studio + gemma 對 `struct Character{name,affection,lines}` **只吐 `{"name":…}`**，另兩個欄位整個省略。**離線 fixture 驗不出來**（假回應是自己寫的、欄位當然齊），只有打真後端才現形。
  **對策＝開 opts，不是自己補**：glaze 本來就有這能力，判準寫在它 `core/common.hpp` 的 `requires_key`——「**開了 `error_on_missing_keys`、且該成員非 nullable** ⇒ 列入 `required`」。所以 `glz::write_json_schema<T, glz::opts{.error_on_missing_keys = true}>()` 就自動長出 `required`，**而且語意是對的**：`std::optional<…>` 這種本來就該選填的欄位會被**正確排除**在 required 外（實測 `struct WithOpt{name; optional<string> nickname; age;}` → `required:["name","age"]`，nickname 型別還自動變 `["string","null"]`）。⚠ **別自己把 `properties` 的鍵全塞進 `required`**——那會把真正選填的欄位錯標成必填（第一版就是這樣寫的，已拆掉）。
  ⚠ **凡是「從 struct 反射生 schema 給模型」的地方都要一起改**：try_3 踩到時只想到結構化輸出（`ask_as`），但**工具呼叫的參數 schema**（`make_tool<Args>()`）是同一個坑——工具參數也會變全選填、模型可以少給。已抽成 `src/llm_schema.hpp` 的 `schema_of<T>()`＋`kSchemaOpts`，兩處共用。

- **離線 fixture 被 `.gitignore` 蓋掉＝測試在新 clone 上根本跑不了**：try_1 的 `.gitignore` 曾把 `fake/` 整個 ignore，於是假回應 JSON 從沒進版控，換一台機器 clone 下來 `test.scm` 直接死（curl 找不到檔）。**fixture 是測試資料、不是 build 產物，該進版控**（try_2 的 `test/fixtures/` 一開始就做對）。已扶正。
