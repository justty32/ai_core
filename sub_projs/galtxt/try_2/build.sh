#!/usr/bin/env bash
# build.sh — 重建 liblua.a（vendored Lua 5.5 原始碼）、host.exe（內嵌 Lua 的 C++ host）、
#            lua.exe（標準 Lua 直譯器，給 VSCode debug／一般跑腳本用；內建 arg 表）
# 用（Git Bash）：./build.sh          # 全建（liblua.a + host.exe + lua.exe）
#                 ./build.sh host    # 只重建 host.exe（liblua.a 已在時最常用）
#                 ./build.sh luaexe  # 只重建 lua.exe
#                 ./build.sh lua     # 只重建 liblua.a
#
# 需要：MinGW g++/gcc/ar 在 PATH（本機 C:\dev\mingw64\bin）。
set -euo pipefail
export PATH="/c/dev/mingw64/bin:$PATH"
cd "$(dirname "$0")"

build_lua() {
  echo "[build] liblua.a（除 lua.c/luac.c 兩個 main 外全部）"
  cd vendor/lua
  local objs=()
  for f in $(ls *.c | grep -vE '^(lua|luac)\.c$'); do
    gcc -O2 -c "$f" -o "${f%.c}.o"
    objs+=("${f%.c}.o")
  done
  ar rcs liblua.a "${objs[@]}"
  rm -f "${objs[@]}"
  echo "[build] liblua.a 完成"
  cd ../..
}

build_host() {
  echo "[build] host.exe"
  g++ -std=c++20 -O2 host.cpp -I vendor/lua -L vendor/lua -llua -o host.exe -municode
  echo "[build] host.exe 完成"
}

build_luaexe() {
  echo "[build] lua.exe（標準直譯器 = lua.c + liblua.a）"
  gcc -O2 vendor/lua/lua.c -I vendor/lua -L vendor/lua -llua -lm -o lua.exe
  echo "[build] lua.exe 完成"
}

case "${1:-all}" in
  lua)    build_lua ;;
  host)   build_host ;;
  luaexe) build_luaexe ;;
  all)    build_lua; build_host; build_luaexe ;;
  *) echo "用法：./build.sh [all|lua|host|luaexe]"; exit 1 ;;
esac
echo "[build] OK"
