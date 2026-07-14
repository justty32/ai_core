# try_3 — galtxt 玩具實驗場③：純 C++（C++20 modules）

← [galtxt INDEX](../INDEX.md)

galtxt 第三條實驗線，與 [try_1](../try_1/README.md)（s7 Scheme）、[try_2](../try_2/README.md)（C++ 內嵌 Lua 5.5）並存、互為對照。

**這條完全 C++**——不嵌任何腳本 VM，純原生，並以 **C++20 modules**（`export module` / `import`）當骨架示範。建置走 **CMake + Ninja**（配 `CMakePresets.json` 釘死工具鏈、接 vcpkg toolchain），與 try_2 的手寫 `build.sh` 走不同路線，兩相對照。

目前狀態：**可建置、可 VSCode/gdb 除錯的最小骨架**。`src/demo.cppm` 是具名模組 `demo`（`sum_to` 迴圈累加、`greet` 字串處理），`src/main.cpp` 以 `import demo;` 取用。

## 需要的東西

- **MinGW-w64**（本機 `C:/dev/mingw64`）：g++ 16（支援 C++20 modules）、gdb。
- **CMake** 3.28+（本機 4.3.3；modules 需 3.28+）。
- **Ninja** 1.11+（本機用 vcpkg 自帶的 **1.13.2**）。★ **modules 必須用 Ninja 或 VS generator——MinGW Makefiles 不支援 module 掃描**（CMake 會直接報錯）。
  取得：`C:/dev/vcpkg/vcpkg.exe fetch ninja` 會下載並印出路徑（本機在 `C:/dev/vcpkg/downloads/tools/ninja-1.13.2-windows/ninja.exe`，已釘進 preset）。
- **vcpkg**（本機 `C:/dev/vcpkg`）：preset 已接 `scripts/buildsystems/vcpkg.cmake` toolchain，日後加依賴（`vcpkg.json` manifest）即自動安裝。
- **VSCode 擴充**：
  - **C/C++**（`ms-vscode.cpptools`）— 必裝，提供 `cppdbg` 除錯器。
  - **CMake Tools**（`ms-vscode.cmake-tools`）— 選裝，裝了能在側欄切 preset／建置。

路徑（`C:/dev/mingw64`、`C:/dev/vcpkg`、ninja 版本）都在 `CMakePresets.json` 與 `.vscode/` 釘死，換機改那幾處即可。

## 建置（命令列）

```bash
cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
cmake --build --preset mingw-debug  # 建置 → build/try3.exe
./build/try3.exe                    # 跑（預設 N=10）
./build/try3.exe 5 星野             # N=5、名字＝星野
```

Release：把 `mingw-debug` 換成 `mingw-release`。CMake 會自動掃 `import`/`export`、生 dyndep，**先編 `demo.cppm` 成 BMI（`demo.gcm`）再編 `main`**。

## 加 GitHub 依賴（vcpkg manifest）— 下一步

vcpkg toolchain 已在 preset 接好，用 **manifest 模式** 加庫（宣告式、隨專案版控）：

1. try_3 根建 `vcpkg.json`：
   ```json
   { "name": "try-3", "version-string": "0.0.1", "dependencies": ["fmt"] }
   ```
2. **★ MinGW triplet（最容易踩的雷）**：vcpkg 預設 triplet 是 `x64-windows`（MSVC 導向），在 MinGW 上會建置／連結失敗。在 `CMakePresets.json` 的 `cacheVariables` 補（選 static，與本線 `-static` 獨立 exe 哲學一致）：
   ```json
   "VCPKG_TARGET_TRIPLET": "x64-mingw-static",
   "VCPKG_HOST_TRIPLET":   "x64-mingw-static"
   ```
   （要動態庫改 `x64-mingw-dynamic`，但那樣 exe 執行期要找 DLL，與本線 `-static` 相左。）
3. `CMakeLists.txt` 用庫：`find_package(fmt CONFIG REQUIRED)` ＋ `target_link_libraries(try3 PRIVATE fmt::fmt)`。
4. 重配置 `cmake --preset mingw-debug` → vcpkg 從源碼建依賴、自動裝（首次較久）。

> ⚠ **第一次裝庫先挑小庫（如 `fmt`）** 驗整條 vcpkg→MinGW→靜態連結通不通，再往上堆設計——triplet／靜態連結的雷這時才會現形，先在小範圍打通。**本設定 Windows 尚未實跑過任何 vcpkg 庫。**

## VSCode 除錯

以「**File > Open Folder**」開 `try_3` 這個資料夾（讓 `${workspaceFolder}=try_3`），然後：

- **Ctrl+Shift+B** → 跑預設任務「cmake: build」建置。
- **F5** → 「Debug try3 (gdb/MinGW)」：F5 前會自動建置（`preLaunchTask`），在 `demo.cppm` 的 `sum_to` 迴圈、`greet` 或 `main.cpp` 的 `run` 下中斷點試單步、看變數。

`.vscode/` 四個檔：`tasks.json`（cmake configure/build/run，走 `--preset`、與 generator 無關）、`launch.json`（cppdbg + MinGW gdb）、`settings.json`（CMake/UTF-8）、`c_cpp_properties.json`（IntelliSense 讀 `build/compile_commands.json`）。

## C++20 modules 重點與坑

- **generator 必須 Ninja/VS**：Makefiles 不支援 module 掃描（CMake 明講）。故 preset 用 Ninja。
- **CMake 接法**：`target_sources(try3 PRIVATE FILE_SET CXX_MODULES FILES src/demo.cppm)`——CMake 據此掃相依、排建置序（先 BMI 再引用者）。
- **⚠ 除錯中斷點的模組修飾**：C++20 具名模組 export 的函式，符號名帶模組修飾（gdb 顯示 `sum_to@demo`）。
  - **VSCode 行號槽中斷點（file:line）完全正常**——這是最常用的方式，不受影響。
  - 若在 gdb/除錯主控台**按函式名**下中斷，純 `break sum_to` 找不到，要 `break sum_to@demo` 或改用 `break demo.cppm:16`（file:line 最穩）。

## 設計要點（吸取 try_2 踩過的坑）

- **`-static` 靜態連結** libstdc++/libgcc/winpthread → 產出獨立 exe，**不依賴 `C:/dev/mingw64/bin` 在 PATH 上**。否則 gdb 啟動 exe 時會找不到 mingw runtime DLL 而失敗。（實測 `objdump -p` 只剩 KERNEL32＋系統 CRT。）
- **中文不亂碼**：`wmain` + `-municode` 取寬字元命令列（中文 argv 不壞）＋ `SetConsoleOutputCP(CP_UTF8)`。跨平台：Windows 專屬碼以 `#ifdef _WIN32` 包起，Linux 走標準 `main`＋原生 UTF-8 argv。
- **⚠ SAC（Smart App Control）偶發封鎖**：新編 exe 首次執行可能被「應用程式控制原則」擋（`Permission denied` / 「應用程式控制原則已封鎖此檔案」）。重新連結（產生新 hash）即放行——與 try_2 同一個坑，詳見 [common/gotchas](../workflows/common/gotchas.md)。
- **⚠ 命令列 gdb 引號坑**：從 Git Bash 直接跑原生 `gdb.exe` 時，多字詞 `-ex "break foo"` 的引號分組會遺失。要在 shell 手動驅動 gdb 改用命令檔 `gdb --batch -x cmds.gdb`。**VSCode 的 cppdbg 走 MI 協定、不經 shell，不受此影響。**

## 跨平台（Windows ⇄ Manjaro 雙機）

工具鏈路徑目前寫死 Windows（`C:/dev/mingw64`、`C:/dev/vcpkg` 的 ninja、vcpkg toolchain）。回 Manjaro 需把 `CMakePresets.json`、`.vscode/*.json` 的編譯器／gdb／ninja／toolchain 路徑改成 Linux 原生：

- g++（同樣支援 C++20 modules）、gdb：`/usr/bin/...`。
- ninja：`sudo pacman -S ninja`（→ `/usr/bin/ninja`），或一樣用 `vcpkg fetch ninja`。
- vcpkg：clone 到家機路徑，toolchain 指該處 `scripts/buildsystems/vcpkg.cmake`。

或把 preset 拆出 Linux 版（`inherits` 共用設定、只覆寫路徑）。`.vscode/` 僅 Windows VSCode 用；Manjaro 若改用 neovim/lazyvim，除錯設定另於該環境自建。**本骨架只在 Windows 實測過。**

> vcpkg 在 MinGW 上加依賴時，triplet 記得設 `x64-mingw-dynamic`（或 `-static`），別用預設的 `x64-windows`（那是 MSVC 導向）。目前無依賴、toolchain 呈惰性，尚不影響。
