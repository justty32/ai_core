#!/usr/bin/env bash
# build.sh — 建置全部產物到 build/：liblua.a（vendored Lua 5.5）、host.exe（內嵌 Lua 的 C++ host）、
#            lua.exe（標準 Lua 直譯器，給 VSCode debug／一般跑腳本用；內建 arg 表）
# 用（Git Bash）：./build.sh          # 全建（build/liblua.a + build/host.exe + build/lua.exe）
#                 ./build.sh host    # 只重建 host.exe（liblua.a 已在時最常用）
#                 ./build.sh luaexe  # 只重建 lua.exe
#                 ./build.sh lua     # 只重建 liblua.a
#
# 需要：MinGW g++/gcc/ar 在 PATH（本機 C:\dev\mingw64\bin）。產物全落 build/（gitignored）。
set -euo pipefail
export PATH="/c/dev/mingw64/bin:$PATH"
cd "$(dirname "$0")"
mkdir -p build

build_lua() {
  echo "[build] build/liblua.a（除 lua.c/luac.c 兩個 main 外全部）"
  local objs=()
  for f in $(ls vendor/lua/*.c | grep -vE '/(lua|luac)\.c$'); do
    gcc -O2 -c "$f" -o "build/$(basename "${f%.c}").o"
    objs+=("build/$(basename "${f%.c}").o")
  done
  ar rcs build/liblua.a "${objs[@]}"
  rm -f "${objs[@]}"
  echo "[build] build/liblua.a 完成"
}

build_host() {
  echo "[build] build/host.exe"
  g++ -std=c++20 -O2 host.cpp -I vendor/lua -L build -llua -o build/host.exe -municode
  echo "[build] build/host.exe 完成"
}

build_luaexe() {
  echo "[build] build/lua.exe（標準直譯器 = lua.c + liblua.a）"
  gcc -O2 vendor/lua/lua.c -I vendor/lua -L build -llua -lm -o build/lua.exe
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
