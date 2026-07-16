# CMakeLists.txt — galtxt try_4（三線整合：try_3 純 C++ 核心當底，嵌 s7＋Lua 薄層）
#
# ★ 定位：try_1（s7 Scheme）、try_2（內嵌 Lua）、try_3（純 C++）三條實驗線合流。
#   C++ 核心扛重活（傳輸／glaze 反射／schema 生成／ask 及三擴充），s7 與 Lua 只當**薄薄一層**、
#   吃 C++ 開出來的 function API。各取所長：C++ 靜態反射＋原生效能，腳本 REPL／同像性／改一行就跑。
#
# ★★ 貫穿本檔的一句話＝「借、不分叉」：核心與兩個 VM 的原始碼**都不複製**，一律「借編」——
#     - try_3 核心 .cpp：借 ../try_3/src（除 main/cli/demo），try_3 一字不改保純基準線。
#     - Lua 5.5：借 ../try_2/vendor/lua/*.c（除它家改過的 linit.c），只覆蓋 linit（見 src/lua_linit_clean.c）。
#     - s7：借外部 s7-playground 的 s7.c（同 try_1，不在本 repo；S7_DIR 指定）。
#     一份原始碼、多處借編：任一核心一改，借它的線同時跟上、不會漂成多套。
#
# 用（與 try_3 同一套 preset 名）：
#   cmake --preset mingw-debug          # 首次 configure（Linux 用 linux-debug）
#   cmake --build --preset mingw-debug  # → build/try4.exe
#   （s7 源碼另置：預設找 C:/code/mine/pas/derived/s7-playground；或 -DS7_DIR=<路徑> 指定。）

cmake_minimum_required(VERSION 3.28)
project(try_4 LANGUAGES CXX C)          # 多了 C：s7.c 與 Lua vendored 都是 C

set(CMAKE_CXX_STANDARD 23)              # glaze 需 C++23（g++16 支援）——與 try_3 對齊
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)               # Lua／s7 是 C11 相容
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # 給 clangd／cpptools 讀正確編譯旗標

find_package(glaze CONFIG REQUIRED)    # 反射式 JSON↔struct（header-only）

# ════════════════════════════════════════════════════════════════════════════
# (A) try_3 核心來源（借編、不複製）
# ════════════════════════════════════════════════════════════════════════════
# ★ try_3 於 2026-07 重構成「對外 C ABI（libgaltxt.so）」，把可獨立呼叫的 L0 C++ 入口
#   （llm::Client／ask／ask_tools／ask_vision／ask_as）連同 demo 一起封存進 ../try_3/archived/，
#   src/ 只留 C ABI（cabi.*）＋傳輸管子 http.*。try_4 這條線仍吃舊 L0 的 C++ function API，故
#   從 archived/ 借那四個 .cpp（內容一字未改，只是換了目錄）；http.cpp 仍在 src/。
# ★ include 路徑要同時涵蓋 src/（archived/llm.cpp 仍 #include "http.hpp"，http.hpp 留在 src/）
#   與 archived/（try_4 自家 main.cpp／bind_bridge.cpp include llm*.hpp）——見下方 target_include_directories。
set(TRY3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../try_3")
set(TRY3_CORE
    "${TRY3_DIR}/src/http.cpp"            # native HTTP 傳輸（WinHTTP／libcurl；file:// 離線特例）
    "${TRY3_DIR}/archived/llm.cpp"        # llm::Client：struct＋反射 ask 接口（重構後封存於 archived/）
    "${TRY3_DIR}/archived/llm_tool.cpp"   # 工具呼叫（function calling）
    "${TRY3_DIR}/archived/llm_media.cpp"  # 多媒體／vision
    "${TRY3_DIR}/archived/llm_json.cpp")  # 結構化輸出（ask_as<T>）

# ════════════════════════════════════════════════════════════════════════════
# (B) Lua 5.5（借編 try_2 的 vendored 原始碼；只覆蓋它家改過的 linit.c）→ static lib
# ════════════════════════════════════════════════════════════════════════════
set(LUA_VENDOR "${CMAKE_CURRENT_SOURCE_DIR}/../try_2/vendor/lua")
file(GLOB LUA_SRC "${LUA_VENDOR}/*.c")
# 排除：lua.c/luac.c（各自帶 main）＋ linit.c（try_2 改版含 cjson/http preload，見 src/lua_linit_clean.c）
list(FILTER LUA_SRC EXCLUDE REGEX "/(lua|luac|linit)\\.c$")
add_library(lua_vendored STATIC ${LUA_SRC} "${CMAKE_CURRENT_SOURCE_DIR}/src/lua_linit_clean.c")
# PUBLIC：連 lua_vendored 的目標（try4）也拿到 lua.h 的 include 路徑
target_include_directories(lua_vendored PUBLIC "${LUA_VENDOR}")
if(NOT WIN32)
    # luaconf.h 只在 _WIN32 自動開 LUA_USE_WINDOWS；POSIX 這邊要自己給，否則 io.popen／os.tmpname
    # 退成錯誤樁（見 try_2 build.sh 的 LUAPLAT 註）。LUA_USE_LINUX 連帶開 dlopen → 連結需 -ldl。
    target_compile_definitions(lua_vendored PRIVATE LUA_USE_LINUX)
endif()

# ════════════════════════════════════════════════════════════════════════════
# (C) s7 Scheme（借外部 s7-playground 的 s7.c；同 try_1）→ static lib
# ════════════════════════════════════════════════════════════════════════════
# s7.c/s7.h 不在本 repo（來自另一顆 repo s7-playground）。S7_DIR 指定；未給就試平台候選路徑。
set(S7_DIR "" CACHE PATH "s7-playground 目錄（含 s7.c/s7.h）")
if(NOT S7_DIR)
    foreach(cand
            "C:/code/mine/pas/derived/s7-playground"      # 本機 Windows（同 try_1 build.sh 候選）
            "$ENV{HOME}/repo/pas/derived/s7-playground")  # Linux 候選
        if(EXISTS "${cand}/s7.c")
            set(S7_DIR "${cand}")
            break()
        endif()
    endforeach()
endif()
if(NOT EXISTS "${S7_DIR}/s7.c")
    message(FATAL_ERROR
        "找不到 s7.c。s7 原始碼不在本 repo，請去 s7-playground 跑 setup.sh 抓回 s7.c/s7.h，"
        "再用 -DS7_DIR=<該目錄> configure（試過：${S7_DIR}）。")
endif()
message(STATUS "try_4: 用 s7 源碼 ${S7_DIR}/s7.c")
add_library(s7_vendored STATIC "${S7_DIR}/s7.c")
target_include_directories(s7_vendored PUBLIC "${S7_DIR}")
# WITH_C_LOADER=0：關掉 s7 的動態 C 模組載入（薄層用不到），省掉 -ldl／-Wl,-export-dynamic 的連結牽扯。
# 不定義 WITH_MAIN：否則撞 s7 自帶的 main()。
target_compile_definitions(s7_vendored PRIVATE WITH_C_LOADER=0)
if(WIN32)
    # MinGW 缺 <sys/utsname.h>，s7.c 的 g_uname 無條件 include 它 → 借 try_1 的 shim（僅此 lib、僅 Windows：
    # Linux 有真的 <sys/utsname.h>，絕不可加這條 shim 路徑蓋掉它，見 try_1 build.sh 註）。
    target_include_directories(s7_vendored PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../try_1/shim_include")
endif()

# ════════════════════════════════════════════════════════════════════════════
# (D) try_4 本體：雙 VM host（main 分派）＋綁定三 TU＋借編的 try_3 核心
# ════════════════════════════════════════════════════════════════════════════
add_executable(try4
    src/main.cpp
    src/bind_bridge.cpp   # 共用引擎：唯一碰 llm::Client 的橋（noexcept 收斂例外）
    src/lua_bind.cpp      # Lua 綁定＋host：註冊 llm.ask、跑 .lua
    src/s7_bind.cpp       # s7 綁定＋host：定義 llm-ask、跑 .scm
    ${TRY3_CORE})
target_include_directories(try4 PRIVATE                       # 借編核心的 header：
    "${TRY3_DIR}/src"                                         #   http.hpp（傳輸管子仍在 src/）
    "${TRY3_DIR}/archived")                                   #   llm*.hpp（重構後封存於 archived/）
target_link_libraries(try4 PRIVATE glaze::glaze lua_vendored s7_vendored)

set_target_properties(try4 PROPERTIES SUFFIX ".exe")   # 跨平台指令一致（對齊 try_2/try_3）

# 注入 try_3 目錄，讓 main.cpp 離線煙霧測試用 file:// 讀 try_3/test/fixtures/（連假回應都借）
target_compile_definitions(try4 PRIVATE TRY4_TRY3_DIR="${TRY3_DIR}")

target_compile_options(try4 PRIVATE
    $<$<CONFIG:Debug>:-g;-O0>
    $<$<CONFIG:Release>:-O2>)

if(WIN32)
    # 對齊 try_3：-municode 取寬字元命令列（中文 argv 不亂碼）、-static* 產獨立 exe。
    target_link_options(try4 PRIVATE
        -municode
        -static -static-libgcc -static-libstdc++)
    target_link_libraries(try4 PRIVATE winhttp)   # http.cpp 的 WinHTTP 傳輸
else()
    find_package(CURL REQUIRED)
    target_link_libraries(try4 PRIVATE CURL::libcurl ${CMAKE_DL_LIBS} m)   # curl＋dl（Lua）＋m（數學）
endif()
