# 除錯與編輯器整合

← [README](../README.md)｜[docs 索引](README.md)｜[建置環境](setup.md)｜[平台與傳輸](platform.md)

VSCode/gdb 除錯、clangd（nvim/VSCode 共用）LSP 設定、以及靜態連結／中文／SAC 等設計要點。

## VSCode 除錯

以「**File > Open Folder**」開 `cllm` 這個資料夾（讓 `${workspaceFolder}=cllm`），然後：

- **Ctrl+Shift+B** → 跑預設任務「cmake: build」建置。
- **F5** → gdb/MinGW 除錯：F5 前會自動建置（`preLaunchTask`），在 `cli.cpp` 的 `cli::run`、`main.cpp` 的入口或 `cabi_request.cpp` 的 `build_body` 下中斷點試單步、看變數。傳統 header 下符號名無模組修飾，`break cli::run` 直接命中（不像具名模組要 `run@cli`）。⚠ `.vscode/launch.json` 的除錯目標若還指著舊 `try3` exe，換機時記得改指 `llm`（舊 demo exe 已封存）。

`.vscode/` 四個檔：`tasks.json`（cmake configure/build/run，走 `--preset`）、`launch.json`（cppdbg + MinGW gdb）、`settings.json`（CMake/UTF-8）、`c_cpp_properties.json`（IntelliSense 讀 `build/compile_commands.json`）。

## 編輯器整合（clangd／nvim／VSCode 共用一份設定）

**LSP／IntelliSense 的真相源是 `build/compile_commands.json`**：`CMakeLists.txt` 開了 `CMAKE_EXPORT_COMPILE_COMMANDS ON`，所以只要 `cmake --preset <xxx>` configure 過一次，這份檔案就在 `build/` 底下（`build/` 整包 gitignored，換機/清乾淨重建都會自動再生，不用手動維護）。VSCode 端 `c_cpp_properties.json` 本來就指到這份檔；nvim 端（LazyVim + clangd）另外靠下面這個檔接上。

- **[`.clangd`](../.clangd)**（版控、根目錄）：clangd 預設只會往「當前檔案所在目錄」往上層找 `compile_commands.json`，不會自動探進 `build/`，所以明講 `CompilationDatabase: build`。這是 clangd 原生設定格式，nvim 的 clangd client 和 VSCode 的 clangd 擴充都吃——**一份設定兩邊共用、也進版控**，比在各編輯器設定裡各塞一次 `--compile-commands-dir` 乾淨。
  - vcpkg 的 include 路徑（`build/vcpkg_installed/x64-linux/include`）**不用額外處理**：`compile_commands.json` 裡每條指令本來就帶 `-isystem .../vcpkg_installed/x64-linux/include`，clangd 讀資料庫時原樣套用，實測過（`src/cabi_request.cpp` 這種吃 glaze 的檔案沒有 `glaze/glaze.hpp` 找不到的診斷）。
  - `.clangd` 還設了 `Remove`，濾掉 `-fmodules-ts`／`-fmodule-mapper=*`／`-fdeps-format=*` 三個旗標——這是 CMake＋Ninja 為了 C++20 modules 依賴掃描（P1689）自動塞給每個編譯單元的（即使本專案早就回歸傳統 header、不用 modules，Ninja generator 仍會加），clang 驅動器不認得會冒出 `Unknown argument` 診斷雜訊（不影響實際程式碼分析，純視覺雜訊，過濾掉即可）。詳見 [gotchas/build](../workflows/common/gotchas/build.md)。
  - **VSCode 也吃這份 `.clangd`**：若改用 clangd 擴充（`llvm-vs-code-extensions.vscode-clangd`）取代 cpptools 的 IntelliSense，會自動套用同一份設定；目前 VSCode 側仍走 cpptools＋`c_cpp_properties.json`（見上「VSCode 除錯」一節），`.clangd` 是為 nvim 準備的，兩邊不衝突。
- **nvim（LazyVim）**：clangd 由 nvim-lspconfig 依附 C/C++ 檔案自動啟動，讀到根目錄的 `.clangd` 就會用 `build/compile_commands.json`；`CMakeLists.txt` 會被 `neocmakelsp` 接手（獨立 LSP，不需要額外設定）。實測：`nvim --headless src/cabi_request.cpp` 等 clangd 索引完，`vim.lsp.get_clients()` 看得到 `clangd`、`vim.diagnostic.get(0)` 乾淨（無 include 錯誤、無旗標雜訊）。
- **除錯要先自己 build**：VSCode 的 `launch.json` 用 `preLaunchTask` 在 F5 前自動跑 `cmake --build --preset mingw-debug`；**nvim 的 dap 不會跑這個 preLaunchTask**（那是 VSCode 專屬機制），用 nvim 除錯前要自己先手動 `cmake --build --preset linux-debug`（或對應平台的 preset）建一次，之後改了程式碼記得重建，不然中斷點會對不上舊的執行檔。
- `.vscode/launch.json` 的 `"type": "cppdbg"` 兩邊都不用改：nvim 端把 `cppdbg` 這個 adapter 名稱另外別名到 codelldb（設定在使用者的 nvim config repo，不在本專案範圍內）。`miDebuggerPath`（寫死 `C:/dev/mingw64/bin/gdb.exe`）、`MIMode`、`setupCommands` 幾欄是 cppdbg（gdb/MI）專屬，codelldb 會直接忽略，不影響 nvim 側除錯；檔案內已加註解說明。（nvim-dap／codelldb 的坑見 [gotchas/build](../workflows/common/gotchas/build.md)。）

## 設計要點

- **`-static` 靜態連結** libstdc++/libgcc/winpthread → 產出獨立 exe，**不依賴 `C:/dev/mingw64/bin` 在 PATH 上**。否則 gdb 啟動 exe 時會找不到 mingw runtime DLL 而失敗。（實測 `objdump -p` 只剩 KERNEL32＋系統 CRT。）
- **中文不亂碼**：`wmain` + `-municode` 取寬字元命令列（中文 argv 不壞）＋ `SetConsoleOutputCP(CP_UTF8)`。跨平台：Windows 專屬碼以 `#ifdef _WIN32` 包起，Linux 走標準 `main`＋原生 UTF-8 argv。
- **⚠ SAC（Smart App Control）偶發封鎖**：新編 exe 首次執行可能被「應用程式控制原則」擋（`Permission denied` / 「應用程式控制原則已封鎖此檔案」）。重新連結（產生新 hash）即放行，詳見 [gotchas/windows](../workflows/common/gotchas/windows.md)。
- **⚠ 命令列 gdb 引號坑**：從 Git Bash 直接跑原生 `gdb.exe` 時，多字詞 `-ex "break foo"` 的引號分組會遺失。要在 shell 手動驅動 gdb 改用命令檔 `gdb --batch -x cmds.gdb`。**VSCode 的 cppdbg 走 MI 協定、不經 shell，不受此影響。**
