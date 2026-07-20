#!/usr/bin/env bash
# build.sh — 編 cllm 的 Janet native 模組（llm.so）。需先建好 ../../build/libcllm.so。
#   JANET_INC 覆寫 janet 頭檔目錄（預設 ~/.local/include/janet，即含 janet.h 的目錄）。
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"   # cllm 專案根
JANET_INC="${JANET_INC:-$HOME/.local/include/janet}"

[ -f "$ROOT/build/libcllm.so" ] || { echo "先建 libcllm.so：cmake --build --preset linux-debug" >&2; exit 1; }
[ -f "$JANET_INC/janet.h" ] || { echo "找不到 $JANET_INC/janet.h —— 用 JANET_INC 指到含 janet.h 的目錄" >&2; exit 1; }

gcc -O2 -fPIC -shared \
  -I"$JANET_INC" -I"$ROOT/src" \
  "$HERE/llm.c" -o "$HERE/llm.so" \
  -L"$ROOT/build" -lcllm -Wl,-rpath,"$ROOT/build"

echo "built $HERE/llm.so"
echo "試跑：JANET_PATH='$HERE' janet '$HERE/example.janet' \"file://$ROOT/test/fixtures/\""
