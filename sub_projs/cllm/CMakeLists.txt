# CMakeLists.txt — cllm：對外 C ABI 共享庫 libcllm.so ＋ `llm` CLI 執行檔
#
# 建置路線：CMake（配 CMakePresets.json 釘死工具鏈）＋Ninja＋vcpkg toolchain。
#
# 用（命令列）：
#   cmake --preset linux-debug          # 首次 configure（讀 CMakePresets.json）
#   cmake --build --preset linux-debug  # 建置 → build/libcllm.so ＋ build/llm
#
# 兩個交付物：
#   (1) build/libcllm.so —— 對外 C ABI（src/cabi.h，extern "C"）。純 C 客戶端 include
#       src/cabi.h、連 -lcllm 即可用。唯一入口 llm_ask 統一吃 prompt＋schema＋tools＋media＋
#       modalities＋stream；請求組裝的唯一真相源在 src/cabi.cpp。曾散在 L0 的獨立 C++ 入口
#       （ask／ask_tools／ask_vision／ask_json）已封存進 archived/，不再對外提供。
#   (2) build/llm —— unix filter 執行檔（src/main.cpp＋src/cli*.cpp）。走 C++ 鏡像 cabi.hpp
#       消費 libcllm：prompt 走位置參數／stdin，答案吐 stdout，退出碼 0/1/2/130。見 src/cli.hpp。

cmake_minimum_required(VERSION 3.28)
project(cllm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)             # glaze 需 C++23（g++16 支援）
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 匯出 compile_commands.json：給 clangd 讀正確的編譯旗標（IntelliSense 準）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# vcpkg 依賴（manifest 模式，見 vcpkg.json）：glaze＝反射式 JSON↔struct、header-only、超快
find_package(glaze CONFIG REQUIRED)

# ── [L1] 對外 C ABI 共享庫：libcllm.so ──
# 對外介面 src/cabi.h（傘檔，按功能拆成 cabi_client/request/response/context.h）。
# 實作按關注點拆檔（見 src/cabi_internal.hpp 的分工表）：
#   cabi.cpp          出口 llm_ask/llm_cancel/llm_phase ＋ make_request（連線設定→http::Request）
#   cabi_request.cpp  build_body（組 OpenAI 請求 JSON）
#   cabi_response.cpp dispatch_nonstream／guard_backend_error（解析非串流回應＋錯誤護欄）
#   cabi_stream.cpp   do_stream（串流 SSE 路徑）
#   http.cpp          傳輸管子（WinHTTP／libcurl；file:// 離線特例）
add_library(cllm SHARED
    src/cabi.cpp
    src/cabi_request.cpp
    src/cabi_response.cpp
    src/cabi_stream.cpp
    src/http.cpp)

target_link_libraries(cllm PRIVATE glaze::glaze)

# Debug 帶完整符號、關優化，方便 gdb 單步；Release 開 -O2
target_compile_options(cllm PRIVATE
    $<$<CONFIG:Debug>:-g;-O0>
    $<$<CONFIG:Release>:-O2>)

if(WIN32)
    # MinGW：讓 .dll 自動匯出所有非 static 符號（免逐個 __declspec(dllexport)）；
    # 靜態連 libstdc++/libgcc → 產出獨立 .dll，不依賴 mingw runtime DLL 在 PATH 上。
    set_target_properties(cllm PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_link_libraries(cllm PRIVATE winhttp)
    target_link_options(cllm PRIVATE -static-libgcc -static-libstdc++)
else()
    # 非 Windows：http 模組走 libcurl（vcpkg 裝 curl；見 vcpkg.json 註）
    find_package(CURL REQUIRED)
    target_link_libraries(cllm PRIVATE CURL::libcurl)
endif()

# ── [交付物 2] `llm` CLI 執行檔（unix filter）──
# libcllm.so 之上的薄消費端：走 C++ 鏡像 cabi.hpp（llm::abi::Client），連 libcllm 拿 llm_ask。
#   src/cli.cpp   反射生成旗標＋config 檔＋組 Request→ask（用法／退出碼見 src/cli.hpp 檔頭）
#   src/main.cpp  進入點（Windows wmain 取寬字元命令列；POSIX 標準 main）
# glaze：cli.cpp 靠它反射 llm::abi::Client 的欄位生旗標、並反射解析 config JSON。
add_executable(llm
    src/cli.cpp          # orchestrator：run() 串流程
    src/cli_flags.cpp    # 反射 Client 欄位 → 旗標 + --help
    src/cli_config.cpp   # config 檔三層來源 + 檔案/mime/工具定義/媒體落檔小工具
    src/cli_output.cpp   # 出口 handlers：stdout／tool_calls JSON 行／媒體落檔／stderr
    src/main.cpp)        # 進入點（wmain/main）
target_link_libraries(llm PRIVATE cllm glaze::glaze)
target_compile_options(llm PRIVATE
    $<$<CONFIG:Debug>:-g;-O0>
    $<$<CONFIG:Release>:-O2>)

if(WIN32)
    # -municode：wmain 取寬字元命令列（中文 argv 不亂碼），對齊舊 try_3 exe。
    target_link_options(llm PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

# ── 安裝規則：常駐 prefix（如 ~/dev），變成可 include/link 的東西 ──
# 用：cmake --preset linux-debug -DCMAKE_INSTALL_PREFIX=$HOME/dev
#     cmake --build   --preset linux-debug
#     cmake --install build
# 產出 <prefix>/{include/cllm,lib,bin,lib/pkgconfig}。裝出來的 .so/exe 帶 INSTALL_RPATH
# 指到自己的 libdir，換位置或從任意 cwd 跑都找得到 libcllm.so（免設 LD_LIBRARY_PATH）。
include(GNUInstallDirs)
set_target_properties(cllm llm PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# 對外頭檔（C ABI 傘檔＋功能頭＋C++ 薄鏡像）——不裝內部頭 cabi_internal.hpp／http.hpp。
install(FILES
    src/cabi.h src/cabi_client.h src/cabi_request.h src/cabi_response.h src/cabi_context.h
    src/cabi.hpp src/cabi_request.hpp src/cabi_response.hpp src/cabi_context.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cllm)
install(TARGETS cllm LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS llm  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# pkg-config：下游 `pkg-config --cflags --libs cllm` 即拿到 -I/-L/-lcllm。
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cllm.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/cllm.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cllm.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
