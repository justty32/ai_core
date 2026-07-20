# 建置環境與依賴

← [README](../README.md)｜[docs 索引](README.md)｜[平台與傳輸](platform.md)｜[除錯](debugging.md)

裝哪些工具、vcpkg 依賴怎麼來。建置指令本身在 [README「建置」](../README.md#建置命令列)；跨平台 preset 細節在 [platform.md](platform.md)。

## 需要的東西

**Windows**

- **MinGW-w64**（本機 `C:/dev/mingw64`）：g++ 16、gdb。
- **CMake** 3.28+（本機 4.3.3）。
- **Ninja** 1.11+（本機用 vcpkg 自帶的 **1.13.2**）。preset 用 Ninja（非模組建置亦適用；當初為 modules 掃描而選，回歸 header 後保留不換）。
  取得：`C:/dev/vcpkg/vcpkg.exe fetch ninja` 會下載並印出路徑（本機在 `C:/dev/vcpkg/downloads/tools/ninja-1.13.2-windows/ninja.exe`，已釘進 preset）。
- **vcpkg**（本機 `C:/dev/vcpkg`）：preset 已接 `scripts/buildsystems/vcpkg.cmake` toolchain，日後加依賴（`vcpkg.json` manifest）即自動安裝。
- **VSCode 擴充**：
  - **C/C++**（`ms-vscode.cpptools`）— 必裝，提供 `cppdbg` 除錯器。
  - **CMake Tools**（`ms-vscode.cmake-tools`）— 選裝，裝了能在側欄切 preset／建置。

**Linux（Manjaro，實測）**

- **系統 g++**（本機 `/usr/bin/g++`，16.1.1）：`pacman -S gcc`；gdb 同包路徑取得。
- **CMake** 3.28+（本機 4.3.4）、**Ninja** 1.11+（本機 `/usr/bin/ninja`，1.13.2，`pacman -S cmake ninja`）。
- **vcpkg**（本機 `/home/lorkhan/vcpkg`，設 `VCPKG_ROOT`）：preset 接同一份 `scripts/buildsystems/vcpkg.cmake` toolchain。
- **libcurl**：走**系統既有**（本機 `pacman` 裝的 8.21.0，`pkg-config --modversion libcurl` 可查）——`vcpkg.json` **不加** `curl`（那會從源碼編、很慢），`find_package(CURL REQUIRED)` 在 vcpkg toolchain 下一樣找得到系統庫（見 [platform.md](platform.md)）。
- neovim/lazyvim 除錯另建，`.vscode/` 是 Windows VSCode 專用不適用。

路徑（`C:/dev/mingw64`、`C:/dev/vcpkg`、ninja 版本／`/usr/bin/g++`、`/home/lorkhan/vcpkg`、`/usr/bin/ninja`）都在 `CMakePresets.json`（`mingw-*`／`linux-*` 兩組 preset，`.vscode/` 僅 Windows 側）釘死，換機改那幾處即可。

## 依賴（vcpkg manifest）

已接 **glaze**（反射式 JSON↔struct、header-only、超快）當第一個 vcpkg 庫，兼作整條鏈的驗證。用 **manifest 模式**（宣告式、隨專案版控）：

1. [`vcpkg.json`](../vcpkg.json) 宣告依賴：
   ```json
   { "name": "cllm", "version-string": "0.0.1", "dependencies": ["glaze"] }
   ```
2. **★ MinGW triplet（最容易踩的雷）**：vcpkg 預設 triplet 是 `x64-windows`（MSVC 導向），在 MinGW 上會建置／連結失敗。`CMakePresets.json` 的 `cacheVariables` 已設（static，與本線 `-static` 獨立 exe 哲學一致）：
   ```json
   "VCPKG_TARGET_TRIPLET": "x64-mingw-static",
   "VCPKG_HOST_TRIPLET":   "x64-mingw-static"
   ```
   （要動態庫改 `x64-mingw-dynamic`，但那樣 exe 執行期要找 DLL，與本線 `-static` 相左。）
   **Linux 側 `linux-*` preset 沒設 `VCPKG_TARGET_TRIPLET`**：讓 vcpkg 用偵測到的預設 `x64-linux` 即可，不用比照 MinGW 特別指定。
3. `CMakeLists.txt`：`find_package(glaze CONFIG REQUIRED)` ＋ `target_link_libraries(cllm PRIVATE glaze::glaze)`（`llm` CLI 另連 `cllm` 拿 `llm_ask`＋`glaze` 做反射）。glaze 需 **C++23**（已把 `CMAKE_CXX_STANDARD` 提到 23，g++16 支援）。
4. `cmake --preset mingw-debug`（或 `linux-debug`）配置時 vcpkg 自動裝依賴（glaze header-only，約數秒；Linux 實測 7.4.0，約 1.4 秒裝完）。vcpkg 裝到 `build/vcpkg_installed/`（gitignored）。

**要再加庫**：`vcpkg.json` 的 `dependencies` 加一個名字 → `find_package`＋`target_link_libraries` → 重配置。實測 glaze 7.8.4：`glz::write_json(struct)` / `glz::read_json(struct, src)` 反射自動映射、中文原樣 UTF-8——`cabi_request.cpp` 的 `build_body` 靠它把請求 struct 寫成 JSON、`cabi_response.cpp` 靠它把回應 JSON 解回 struct。

> ℹ glaze 這種現代 C++ 庫用**編譯期反射**：定義 `struct` 就自動 JSON↔struct，不用寫映射巨集（這正是 C++26 `std::meta` 反射要標準化的東西，glaze 現在就有）。

> ⚠ **glaze 預設遇未知鍵報錯**：真後端回應（OpenAI chat completion）欄位遠多於你 struct 挑的那幾個。用 `glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)` 才會忽略多餘鍵、只填 struct 有的。`cabi_response.cpp`（＋`cabi_stream.cpp`）就是這樣用 `kLenient` opts 把 `choices[0].message.content` 台詞挑出來。（更多 glaze 坑見 [gotchas/build](../workflows/common/gotchas/build.md)。）
