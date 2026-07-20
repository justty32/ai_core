#!/usr/bin/env bash
# build.sh — 編 cllm 的 Lua binding（llm.so）。需先建好 ../../build/libcllm.so。
#   LUA_INC 覆寫 Lua 頭檔目錄（預設 /usr/include，即系統 lua）。
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"   # cllm 專案根
LUA_INC="${LUA_INC:-/usr/include}"

[ -f "$ROOT/build/libcllm.so" ] || { echo "先建 libcllm.so：cmake --build --preset linux-debug" >&2; exit 1; }

gcc -O2 -fPIC -shared \
  -I"$LUA_INC" -I"$ROOT/src" \
  "$HERE/llm.c" -o "$HERE/llm.so" \
  -L"$ROOT/build" -lcllm -Wl,-rpath,"$ROOT/build"

echo "built $HERE/llm.so"
echo "試跑：LUA_CPATH='$HERE/?.so' lua '$HERE/example.lua' \"file://$ROOT/test/fixtures/\""
