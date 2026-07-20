# tools/CMakeLists.txt — 周邊工具的 C++ 建置（由上層 add_subdirectory(tools) 帶入）。
#
# 設計：入站用共用 common/httpd（自寫微型 server）；出站重用 cllm 的 src/http 笨管子。
# 零新外部依賴——只 glaze（JSON）＋curl（出站，已是主專案依賴）＋系統 socket/thread。
#
# 產物：
#   anthropic-proxy —— 執行檔（讓 cllm 直連 Anthropic 的轉發代理）
#   （llm-login —— C-ABI lib＋CLI，後續加入）

find_package(glaze CONFIG REQUIRED)
find_package(Threads REQUIRED)
if(NOT WIN32)
  find_package(CURL REQUIRED)
endif()

# 出站傳輸：重用 cllm 的 http 笨管子（同一份 src/http.cpp）
set(TOOLS_HTTP_SRC ${CMAKE_SOURCE_DIR}/src/http.cpp)
set(TOOLS_SRC_DIR  ${CMAKE_SOURCE_DIR}/src)

# ── anthropic-proxy 執行檔 ──
add_executable(anthropic-proxy
    anthropic-proxy/proxy.cpp
    anthropic-proxy/translate.cpp
    common/httpd.cpp
    ${TOOLS_HTTP_SRC})
target_include_directories(anthropic-proxy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${TOOLS_SRC_DIR})
target_link_libraries(anthropic-proxy PRIVATE glaze::glaze Threads::Threads)
target_compile_options(anthropic-proxy PRIVATE
    $<$<CONFIG:Debug>:-g;-O0>
    $<$<CONFIG:Release>:-O2>)
if(WIN32)
    target_link_libraries(anthropic-proxy PRIVATE winhttp ws2_32)
    target_link_options(anthropic-proxy PRIVATE -static)
else()
    target_link_libraries(anthropic-proxy PRIVATE CURL::libcurl)
endif()

# ── llm-login：C-ABI 共享庫 liblogin.so ＋ CLI ＋ 離線測（就跟 cllm 一樣的模組形態）──
add_library(login SHARED
    llm-login/login.cpp
    llm-login/oauth.cpp
    llm-login/store.cpp
    llm-login/crypto.cpp
    common/httpd.cpp        # 接 OAuth callback
    ${TOOLS_HTTP_SRC})      # 出站換 token
target_include_directories(login PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/llm-login
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common ${TOOLS_SRC_DIR})
target_link_libraries(login PRIVATE glaze::glaze Threads::Threads)
target_compile_options(login PRIVATE $<$<CONFIG:Debug>:-g;-O0> $<$<CONFIG:Release>:-O2>)
if(WIN32)
    set_target_properties(login PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_link_libraries(login PRIVATE winhttp ws2_32 bcrypt)
    # --exclude-libs,ALL：同 libcllm——不對外匯出靜態 libgcc 的 _Unwind_Resume，
    # 否則 g++16.1.0 下與消費端 -static-libgcc 撞 multiple definition（見 common/gotchas）。
    target_link_options(login PRIVATE -static -Wl,--exclude-libs,ALL)
else()
    target_link_libraries(login PRIVATE CURL::libcurl)
endif()

# CLI（消費 liblogin 的 C++ 入口，仿 llm CLI 消費 libcllm）
add_executable(llm-login llm-login/login_cli.cpp)
target_link_libraries(llm-login PRIVATE login glaze::glaze)

# 離線煙霧測試（crypto／URL／config patch／token record；不連網不開瀏覽器）
add_executable(llm-login-test-offline llm-login/test_offline.cpp)
target_link_libraries(llm-login-test-offline PRIVATE login glaze::glaze)

# ── 安裝規則：讓 llm-login 就跟 cllm 一樣是下游可 link 的模組 ──
# 沿用上層 CMakeLists 對 cllm 的同一套機制（configure_file → .pc + install(FILES/TARGETS)，
# 非 install(EXPORT)），保持一致。裝出 <prefix>/{lib/liblogin.so, include/cllm/login_abi.h,
# lib/pkgconfig/llm-login.pc, bin/llm-login}。下游走 `pkg-config --cflags --libs llm-login`
# 即拿到 -I/-L/-llogin。GNUInstallDirs 於上層 CMakeLists（add_subdirectory(tools) 之前）已 include。
#
# 用：cmake --preset linux-debug -DCMAKE_INSTALL_PREFIX=$HOME/dev
#     cmake --build --preset linux-debug
#     cmake --install build
#
# liblogin.so 帶 INSTALL_RPATH 指到自己的 libdir（同 cllm），下游從任意 cwd 都找得到。
set_target_properties(login llm-login PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# 對外頭檔：login_abi.h（純 C 可 include），與 cllm 頭同放 include/cllm。
install(FILES llm-login/login_abi.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cllm)
# 下游可 link 產物：liblogin.so（＋順帶裝 llm-login CLI，同 cllm 裝 llm）。
install(TARGETS login      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS llm-login  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# pkg-config：下游 `pkg-config --cflags --libs llm-login`。
# 檔名＝模組名 llm-login；lib 名仍為 -llogin（target 名 login → liblogin.so）。
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/llm-login/login.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/llm-login.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/llm-login.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ── 聯動 harness demo（cllm 401→攔→llm_login_login→patch config→重試；假上游、免瀏覽器）──
# 自帶 httpd（假 LLM／假 OAuth 上游）＋http（當假瀏覽器打 callback），消費 liblogin 的 C-ABI。
# cllm 端以子行程 exec 出貨的 llm 二進位；本 target 只證明聯動串接、不改 cllm 一行。
add_executable(llm-login-demo-harness
    llm-login/demo_harness.cpp
    common/httpd.cpp
    ${TOOLS_HTTP_SRC})
target_include_directories(llm-login-demo-harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common ${TOOLS_SRC_DIR})
target_link_libraries(llm-login-demo-harness PRIVATE login glaze::glaze Threads::Threads)
target_compile_options(llm-login-demo-harness PRIVATE
    $<$<CONFIG:Debug>:-g;-O0> $<$<CONFIG:Release>:-O2>)
if(WIN32)
    target_link_libraries(llm-login-demo-harness PRIVATE winhttp ws2_32)
    target_link_options(llm-login-demo-harness PRIVATE -static)
else()
    target_link_libraries(llm-login-demo-harness PRIVATE CURL::libcurl)
endif()
