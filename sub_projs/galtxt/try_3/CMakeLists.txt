# CMakeLists.txt — galtxt try_3（純 C++、C++20 modules）
#
# 與 try_2 的手寫 build.sh 走不同建置路線：這條用 CMake（配 CMakePresets.json 釘死工具鏈）。
# ★ C++20 modules 需求：generator 必須是 Ninja/VS（Makefiles 不支援 module 掃描）——
#   故 preset 用 Ninja（vcpkg 自帶的 1.13.2）。CMake 會自動掃 import/export、生 dyndep、
#   先編 .cppm 成 BMI（demo.gcm）再編 main。
#
# 用（命令列）：
#   cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
#   cmake --build --preset mingw-debug  # 建置 → build/try3.exe
# VSCode：Ctrl+Shift+B 建置、F5 除錯（見 .vscode/）。

cmake_minimum_required(VERSION 3.28)   # C++20 named modules 支援門檻
project(try_3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)             # glaze 需 C++23（g++16 支援）；modules 亦相容
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 匯出 compile_commands.json：給 cpptools / clangd 讀正確的編譯旗標（IntelliSense 準）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# vcpkg 依賴（manifest 模式，見 vcpkg.json）：glaze＝反射式 JSON↔struct、header-only、超快
find_package(glaze CONFIG REQUIRED)

add_executable(try3 src/main.cpp)

# ★ 具名模組來源：FILE_SET CXX_MODULES —— CMake 據此掃描 module 相依、排建置序
#   demo＝計算/字串示範；http＝native HTTP 傳輸（WinHTTP/libcurl，file:// 離線可跑）
target_sources(try3 PRIVATE
    FILE_SET CXX_MODULES FILES
    src/demo.cppm
    src/http.cppm)

target_link_libraries(try3 PRIVATE glaze::glaze)

# 注入源碼根目錄，讓 main.cpp 的 http 示範用 file:// 讀 test/fixtures/ 而不寫死機器路徑
target_compile_definitions(try3 PRIVATE TRY3_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Debug 帶完整符號、關優化，方便 gdb 單步；Release 開 -O2
target_compile_options(try3 PRIVATE
    $<$<CONFIG:Debug>:-g;-O0>
    $<$<CONFIG:Release>:-O2>)

if(WIN32)
    # -municode：選 wmainCRTStartup，讓 wmain 收寬字元命令列（中文 argv 不亂碼）
    # -static*：靜態連 libstdc++/libgcc/winpthread → 產出獨立 exe，
    #           不依賴 C:/dev/mingw64/bin 在 PATH 上（gdb 啟動 exe 才不會找不到 runtime DLL）
    target_link_options(try3 PRIVATE
        -municode
        -static -static-libgcc -static-libstdc++)
    # http 模組的 WinHTTP 傳輸：系統內建、Schannel TLS，零額外依賴
    target_link_libraries(try3 PRIVATE winhttp)
else()
    # 非 Windows：http 模組走 libcurl（vcpkg 裝 curl；見 vcpkg.json 註）
    find_package(CURL REQUIRED)
    target_link_libraries(try3 PRIVATE CURL::libcurl)
endif()
