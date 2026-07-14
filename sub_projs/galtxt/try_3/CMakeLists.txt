# CMakeLists.txt — galtxt try_3（純 C++、傳統 header）
#
# 與 try_2 的手寫 build.sh 走不同建置路線：這條用 CMake（配 CMakePresets.json 釘死工具鏈）
# ＋Ninja＋vcpkg toolchain。（早期曾用 C++20 modules，已回歸傳統 header；Ninja 保留，非模組亦適用。）
#
# 用（命令列）：
#   cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
#   cmake --build --preset mingw-debug  # 建置 → build/try3.exe
# VSCode：Ctrl+Shift+B 建置、F5 除錯（見 .vscode/）。

cmake_minimum_required(VERSION 3.28)
project(try_3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)             # glaze 需 C++23（g++16 支援）
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 匯出 compile_commands.json：給 cpptools / clangd 讀正確的編譯旗標（IntelliSense 準）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# vcpkg 依賴（manifest 模式，見 vcpkg.json）：glaze＝反射式 JSON↔struct、header-only、超快
find_package(glaze CONFIG REQUIRED)

# 來源：main＋demo（計算/字串示範）＋http（native HTTP 傳輸）＋llm（llm::Client，struct＋反射 ask 接口）
#       ＋llm_tool（工具呼叫）＋llm_media（多媒體/vision）＋llm_json（結構化輸出）
add_executable(try3
    src/main.cpp
    src/demo.cpp
    src/http.cpp
    src/llm.cpp
    src/llm_tool.cpp
    src/llm_media.cpp
    src/llm_json.cpp)

target_link_libraries(try3 PRIVATE glaze::glaze)

# 產物名一律帶 .exe：Windows 本來就是（MinGW 預設），Linux 顯式加上——
# 與 try_2 build.sh 的慣例一致（Linux 不在意副檔名，換來跨平台指令一致：./build/try3.exe 兩邊通用）。
set_target_properties(try3 PROPERTIES SUFFIX ".exe")

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
