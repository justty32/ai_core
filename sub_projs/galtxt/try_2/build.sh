#!/usr/bin/env bash
# build.sh — 建置全部產物到 build/：liblua.a（vendored Lua 5.5）、host.exe（內嵌 Lua 的 C++ host）、
#            lua.exe（標準 Lua 直譯器，給 debug／一般跑腳本用；內建 arg 表）
# 用（Git Bash / Linux bash）：
#   ./build.sh          # 全建（build/{liblua.a, host.exe, lua.exe}）
#   ./build.sh host     # 只重建 host.exe（liblua.a 已在時最常用）
#   ./build.sh luaexe   # 只重建 lua.exe
#   ./build.sh lua      # 只重建 liblua.a
#
# 跨平台：Windows（MinGW，本機 C:\dev\mingw64\bin）＋ Linux（原生 g++/gcc，如 Manjaro）。
# 產物名一律帶 .exe（Linux 不在意副檔名、指令跨平台一致）；全落 build/（gitignored）。
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build

# ── 平台偵測：Windows 需 MinGW + -municode（wmain 寬字元）＋ WinHTTP（native/http.c）；
#    Linux 原生、HTTP 走 libcurl。HTTPLIBS 連結到 host.exe/lua.exe（liblua.a 含 http.o 引用其符號）。
#
# ★ LUAPLAT：Lua 的平台開關。Windows 上 luaconf.h 由 _WIN32 自動定義 LUA_USE_WINDOWS，
#   POSIX 這邊卻要自己給 -DLUA_USE_LINUX，否則 io.popen 直接是「'popen' not supported」的
#   錯誤樁（liolib.c 的 l_popen 只在 LUA_USE_POSIX 下才接真 popen）、os.tmpname 也退回 tmpnam。
#   LUA_USE_LINUX 只再拉一個 -ldl（readline 是 lua.c 執行期 dlopen，不需 -lreadline）。
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    export PATH="/c/dev/mingw64/bin:$PATH"
    MUNICODE="-municode"
    HTTPLIBS="-lwinhttp"
    LUAPLAT=""          # luaconf.h 見 _WIN32 自己開 LUA_USE_WINDOWS
    PLATLIBS=""
    ;;
  *)
    MUNICODE=""
    HTTPLIBS="-lcurl"
    LUAPLAT="-DLUA_USE_LINUX"
    PLATLIBS="-ldl"     # LUA_USE_LINUX 連帶開 LUA_USE_DLOPEN
    ;;
esac

build_lua() {
  echo "[build] build/liblua.a（vendored Lua 除 lua.c/luac.c ＋ 我方 native/*.c）"
  local objs=()
  # vendored Lua 全部（除兩個 main）＋ native/*.c（cjson＝JSON codec、http＝HTTP 傳輸，經 linit.c 註冊）
  for f in $(ls vendor/lua/*.c | grep -vE '/(lua|luac)\.c$') native/*.c; do
    gcc -O2 $LUAPLAT -c "$f" -I vendor/lua -o "build/$(basename "${f%.c}").o"
    objs+=("build/$(basename "${f%.c}").o")
  done
  ar rcs build/liblua.a "${objs[@]}"
  rm -f "${objs[@]}"
  echo "[build] build/liblua.a 完成"
}

build_host() {
  echo "[build] build/host.exe"
  g++ -std=c++20 -O2 $LUAPLAT host.cpp -I vendor/lua -L build -llua -lm $MUNICODE $HTTPLIBS $PLATLIBS -o build/host.exe
  echo "[build] build/host.exe 完成"
}

build_luaexe() {
  echo "[build] build/lua.exe（標準直譯器 = lua.c + liblua.a）"
  gcc -O2 $LUAPLAT vendor/lua/lua.c -I vendor/lua -L build -llua -lm $HTTPLIBS $PLATLIBS -o build/lua.exe
  echo "[build] build/lua.exe 完成"
}

case "${1:-all}" in
  lua)    build_lua ;;
  host)   build_host ;;
  luaexe) build_luaexe ;;
  all)    build_lua; build_host; build_luaexe ;;
  *) echo "用法：./build.sh [all|lua|host|luaexe]"; exit 1 ;;
esac
echo "[build] OK"
