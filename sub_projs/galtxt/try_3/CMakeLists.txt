# CMakeLists.txt — galtxt try_3（純 C++ 專案）
#
# 與 try_2 的 build.sh 走不同建置路線：這條用 CMake（配 CMakePresets.json 釘死 MinGW 工具鏈），
# 兩相對照。單一 config 產生器（MinGW Makefiles），CMAKE_BUILD_TYPE 控 Debug/Release。
#
# 用（命令列）：
#   cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
#   cmake --build --preset mingw-debug  # 建置 → build/try3.exe
# VSCode：Ctrl+Shift+B 建置、F5 除錯（見 .vscode/）。

cmake_minimum_required(VERSION 3.25)
project(try_3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 匯出 compile_commands.json：給 cpptools / clangd 讀正確的編譯旗標（IntelliSense 準）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(try3 src/main.cpp)

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
endif()
