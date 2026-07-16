# 共通踩坑（跨工作流）

← [INDEX](../../INDEX.md)

不專屬任一工作流、任何人都可能撞到的坑，記/查這裡。條目格式：一條一個坑，**粗體標題 + 一兩句現象與對策**，必要時連結細節。

> 這批坑多數在 cllm 還是 `galtxt/try_3` 純 C++ 實驗線時踩到、抽離後帶過來的（含真後端首驗）；仍成立。

---

## glaze / C++ 建置

- **glaze 反射 JSON 預設遇未知鍵報錯**：`glz::read_json(obj, src)` 用預設選項，只要 `src` 有 `obj` struct 沒有的欄位就整包解碼失敗。解真後端回應（OpenAI chat completion 這種欄位一大堆、你只挑 `choices[0].message.content` 幾個）必踩。對策：`glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)`——忽略多餘鍵、只填 struct 有的。`cabi_response.cpp`（＋`cabi_stream.cpp`）就是這樣用 `kLenient` opts。
- **★ 匿名 namespace 的同名 struct 讓 glaze COMDAT section 跨 TU 撞名（MinGW/GCC）**：多個 `.cpp` 各自在**匿名 namespace** 定義同名 struct（如 `ReqBody`），GCC 把匿名 namespace **一律 mangle 成 `_GLOBAL__N_1`**（不保證每 TU 唯一），於是 glaze 為每個型別生成的 `quoted_key_v` 之類 `static constexpr` 走 COMDAT/weak section，section 名含 `_GLOBAL__N_1::ReqBody` → 跨 TU **撞名**。若兩個 `ReqBody` 欄位不同，連結器報 `duplicate section … has different size`，折疊後**可能給錯 JSON 鍵**（不只是警告，是潛在序列化 bug）。對策：**別用匿名 namespace，改各 TU 唯一命名的內部 namespace**（如 `req_impl`／`resp_impl`／`stream_impl`）＋ `using namespace X;`。

## vcpkg / CMake

- **vcpkg toolchain 不會攔截系統的 `find_package(CURL)`**：怕 vcpkg 搶走而特地在 `vcpkg.json` 加 `curl`（→ 從源碼編、很慢）是多餘的。vcpkg toolchain 只是在標準 CMake 模組搜尋**之前**插入自己的搜尋路徑，沒裝就自然落回系統庫。Manjaro 上 `find_package(CURL REQUIRED)` 直接命中 `/usr/lib/libcurl.so`。
- **vcpkg manifest 沒釘 `builtin-baseline`，兩台機器會解出不同版本**：同一份 `vcpkg.json` 因各機 registry 快照不同而版本漂移（實例：glaze，Windows 7.8.4 / Linux 7.4.0）。API 沒變、行為一致，但**跨機潛在地雷**——要鎖版就加 `builtin-baseline` 或 `version>=`。
- **★ MinGW triplet**：vcpkg 預設 triplet `x64-windows`（MSVC 導向）在 MinGW 上會建置／連結失敗，`CMakePresets.json` 的 `mingw-*` 已設 `x64-mingw-static`。**Linux 側 `linux-*` 不設 `VCPKG_TARGET_TRIPLET`**，用偵測到的預設 `x64-linux` 即可。

## clangd / 編輯器

- **CMake 3.28+ 配 Ninja 會塞 C++20 modules 的掃描旗標，clangd 看不懂**：即使專案完全不用 modules，Ninja generator 仍會給每個編譯單元加 `-fmodules-ts`／`-fmodule-mapper=…`／`-fdeps-format=p1689r5`（P1689 依賴掃描）。這些旗標原樣進 `compile_commands.json`，clang 驅動器不認得 → 每個檔都噴 `Unknown argument` 診斷雜訊。對策：`.clangd` 的 `CompileFlags.Remove` 過濾掉（本專案 `.clangd` 有實例）。
- **clangd 不會自己下探 `build/` 找 `compile_commands.json`**：它只從當前檔案往**上層**找；CMake 產在 `build/` 底下，所以要在專案根放 `.clangd` 寫 `CompilationDatabase: build`。這做法**編輯器中立**（nvim 與 VSCode 的 clangd 都讀），比在編輯器設定裡塞 `--compile-commands-dir` 好。
- **nvim-dap：codelldb 只監聽 `127.0.0.1`（純 IPv4）——adapter 寫 `host = "localhost"` 會在 IPv6 優先的機器上 ECONNREFUSED**：`getaddrinfo("localhost")` 先回 `::1` 時連過去就被拒。**LazyVim 自己的 clangd extra 就是寫 `host = "localhost"`**（多數機器碰巧沒事），照抄就踩。對策：**別給 `host`**（預設 `127.0.0.1`）或明寫 `127.0.0.1`。
- **註冊某個 DAP adapter 名字，會招來 mason 自動下載對應套件**：`mason-nvim-dap` 的 `automatic_installation` 掃 `dap.adapters` 的 **key**，看到 `cppdbg` 就去解析成 `cpptools`（微軟 ~90MB 的 VSCode 擴充）開始下載——即使你只想把 `cppdbg` 接到別的除錯器。對策：`automatic_installation = { exclude = { "cppdbg" } }`。

## Windows 執行 / 中文

- **Windows 中文：終端碼頁 vs 命令列參數，兩個獨立的坑**。① **輸出亂碼**＝終端碼頁非 UTF-8（繁中預設常 950）——對策：exe 啟動 `SetConsoleOutputCP(CP_UTF8)` 自救，或終端 `chcp 65001`。② **中文命令列參數壞**＝標準 `main(argv)` 拿到系統碼頁——對策：`wmain`＋`-municode`（本專案 `main.cpp` 這樣做），或中文走 stdin/檔案而非 argv。
- **Smart App Control（SAC）偶發擋掉剛編好的新 exe**：Windows 11 SAC On 時把**某顆新編、沒信譽的 exe 雜湊**判成 unknown→封鎖，報「應用程式控制原則已封鎖此檔案」。**這不是程式錯誤**。GCC 連結是**決定性的**——原始碼沒變，重連結會**重現同一顆被擋的雜湊**；`cp` 成新檔名也同雜湊照擋。真正可靠：**改變二進位內容**（`strip build/llm.exe` 去符號→新雜湊，一次放行；或真改一行原始碼再編）。`Unblock-File`（MOTW）無關無效。
- **Git Bash 跑剛連結好的 exe 報 `Permission denied`（exit 126），但 PowerShell 跑同一支正常**：這**不是 SAC**（SAC 兩個殼都擋），是 Git Bash 對「剛被 `gcc` 寫出、exec 位元狀態未穩」的 exe 偶發拒跑。**別誤診成 SAC 去重編**——改用 PowerShell/cmd 跑，或 `chmod +x`。判別法：PowerShell 跑得動＝非 SAC。

## 真後端（打通 LM Studio 首驗收）

- **★★★ 方法論：「自己寫的離線 fixture」結構上就測不出「真後端長什麼樣」**。fixture 是「我們以為後端會回什麼」的投影，**只投影「成功」、從不投影「失敗」**——所以它只能驗「我們已經想到的東西」。同一病根連咬多次（schema 少欄位、後端 error JSON、reasoning 空答案）。**對策**：離線 fixture 該**刻意塞進真實世界的髒東西**（跳脫序列、缺欄位、多餘欄位、reasoning_content、**以及 error JSON／4xx 這種失敗回應**）；且**新接口一定要對真後端打一次**才算數（記到 [WAIT_USER](../../WAIT_USER.md)）。
- **★★ 後端 error JSON 別靜默吞成空字串——`choices` 空就回 `{}` 是護欄破洞**（2026-07-15 真後端驗揪出）：OpenAI 相容後端出錯（LM Studio 模型載入失敗、雲端 401/429…）回 `{"error":{"message":…}}`＋HTTP 4xx、**無 `choices`**。若寫「解不動或 `choices` 空 → 回空」，後端錯誤就**被吞成空字串、還報成功**——對「笨模型＋護欄」這正是最該堵的洞。**對策**：解析前先攔——`guard_backend_error(status, raw)`（解 error 物件 throw、或 HTTP≥400 帶狀態碼＋body 片段 throw），非串流派送前攔一次、串流路徑接 `http::stream` 的 status＋累積 raw 再攔一次；`llm_ask` 把例外收斂成 `on_error`＋回 `LLM_ERROR`。**離線 `file://` 回 status=200 且無 error 物件，不受影響**。
- **★ reasoning 模型：`max_tokens` 是「reasoning＋content」一起算的——手動給小值會拿到空答案**：gemma-4-e4b 這類 reasoning 模型把思考鏈放 `message.reasoning_content`、答案才在 `message.content`，**共用同一份 `max_tokens` 預算**。手動給 `max_tokens=600` → 預算被 reasoning 吃光、`finish_reason: length`、`content` 空。症狀「後端明明有回、程式卻拿到空的」極易誤診成解析壞掉——看 `usage.completion_tokens_details.reasoning_tokens` 就知道花去哪。**⚠ 別過度反應**：取樣欄位全選填、沒設就不送＝**預設路徑本來就對**（交後端用 context 上限，實測 `finish_reason: stop`、content 完整）。這坑**只在你手動設了 `max_tokens` 才咬人**。
- **★★ schema 的 `required`：沒列進 `required` 的欄位＝選填，後端可以少給**：JSON Schema 語意下約束解碼只保證「不多給鍵」、**不保證「每個欄位都給」**——實測 LM Studio + gemma 對 `{name,affection,lines}` 的 schema **若沒標 required 只吐 `{"name":…}`**。C ABI 收的是**原始 schema JSON 字串**（`llm_schema_t.json`／`llm_tool_def_t.parameters`，caller 自備），故 `required` 要不要標、標哪些是**寫 schema 檔那方的責任**。**離線 fixture 驗不出**（假回應欄位當然齊）。（舊 L0 曾有 `schema_of<T>()` 用 `error_on_missing_keys` 自動生 required 的便利層，已封存進 `archived/llm_schema.hpp`；反射生成 schema 的便利層留待日後在 `cabi.hpp` 上層重長。）

## 測試資料

- **離線 fixture 別被 `.gitignore` 蓋掉＝測試在新 clone 上根本跑不了**：fixture 是**測試資料、不是 build 產物，該進版控**。本專案 `test/fixtures/` 一開始就做對（`cli_smoke.sh` 全走 `file://`、不連網、17/17）。
