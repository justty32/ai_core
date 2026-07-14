# try_3 — galtxt 玩具實驗場③：純 C++

← [galtxt INDEX](../INDEX.md)

galtxt 第三條實驗線，與 [try_1](../try_1/README.md)（s7 Scheme）、[try_2](../try_2/README.md)（C++ 內嵌 Lua 5.5）並存、互為對照。

**這條完全 C++**——不嵌任何腳本 VM，純原生。建置走 **CMake**（配 `CMakePresets.json` 釘死 MinGW 工具鏈），與 try_2 的手寫 `build.sh` 走不同路線，兩相對照。

目前狀態：**可建置、可 VSCode/gdb 除錯的最小骨架**。`src/main.cpp` 只是幾個好下中斷點的目標（`sum_to` 迴圈累加、`greet` 字串處理），先把除錯環境跑起來，之後往上長真東西。

## 需要的東西

- **MinGW-w64**（本機 `C:/dev/mingw64`）：g++ / gdb / mingw32-make。路徑已在 `CMakePresets.json`、`.vscode/` 釘死，換機改那幾處即可。
- **CMake** 3.25+（本機 4.3.3）。
- **VSCode 擴充**：
  - **C/C++**（`ms-vscode.cpptools`）— 必裝，提供 `cppdbg` 除錯器。
  - **CMake Tools**（`ms-vscode.cmake-tools`）— 選裝，裝了能在側欄切 preset／建置；不裝也能用 `.vscode/tasks.json`。

## 建置（命令列）

```bash
cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
cmake --build --preset mingw-debug  # 建置 → build/try3.exe
./build/try3.exe                    # 跑（預設 N=10）
./build/try3.exe 5 星野             # N=5、名字＝星野
```

Release：把 `mingw-debug` 換成 `mingw-release`。

## VSCode 除錯

以「**File > Open Folder**」開 `try_3` 這個資料夾（讓 `${workspaceFolder}=try_3`），然後：

- **Ctrl+Shift+B** → 跑預設任務「cmake: build」建置。
- **F5** → 「Debug try3 (gdb/MinGW)」：F5 前會自動建置（`preLaunchTask`），在 `main.cpp` 的 `sum_to` 迴圈、`greet`、`run` 下中斷點試單步、看變數。

`.vscode/` 四個檔：`tasks.json`（cmake configure/build/run）、`launch.json`（cppdbg + MinGW gdb）、`settings.json`（CMake/UTF-8）、`c_cpp_properties.json`（IntelliSense 讀 `build/compile_commands.json`）。

## 設計要點（吸取 try_2 踩過的坑）

- **`-static` 靜態連結** libstdc++/libgcc/winpthread → 產出獨立 exe，**不依賴 `C:/dev/mingw64/bin` 在 PATH 上**。否則 gdb 啟動 exe 時會找不到 mingw runtime DLL 而失敗。（實測 `objdump -p` 只剩 KERNEL32＋系統 CRT。）
- **中文不亂碼**：`wmain` + `-municode` 取寬字元命令列（中文 argv 不壞）＋ `SetConsoleOutputCP(CP_UTF8)`（主控台照 UTF-8 讀我們吐的位元組）。跨平台：Windows 專屬碼以 `#ifdef _WIN32` 包起，Linux 走標準 `main`＋原生 UTF-8 argv。
- **⚠ SAC（Smart App Control）偶發封鎖**：新編 exe 首次執行可能被「應用程式控制原則」擋（`Permission denied` / 「應用程式控制原則已封鎖此檔案」）。重新連結（產生新 hash）即放行——與 try_2 同一個坑，詳見 [common/gotchas](../workflows/common/gotchas.md)。
- **⚠ 命令列 gdb 引號坑**：從 Git Bash 直接跑原生 `gdb.exe` 時，多字詞 `-ex "break foo"` 的引號分組會遺失（`-ex "run"` 無空白才沒事）。要在 shell 手動驅動 gdb 改用命令檔 `gdb --batch -x cmds.gdb`。**VSCode 的 cppdbg 走 MI 協定、不經 shell，不受此影響。**

## 跨平台（Windows ⇄ Manjaro 雙機）

工具鏈路徑（`C:/dev/mingw64/...`）目前寫死 Windows；回 Manjaro 需把 `CMakePresets.json`、`.vscode/*.json` 的編譯器／gdb 路徑改成 Linux 原生（`/usr/bin/...`），或把 preset 拆出 Linux 版。`.vscode/` 僅 Windows VSCode 用；Manjaro 若改用 neovim/lazyvim，除錯設定另於該環境自建。**本骨架只在 Windows 實測過。**
