#!/usr/bin/env bash
# build.sh — 編 cllm 的 s7 binding 自帶 host（llm-s7）。需先建好 ../../build/libcllm.so。
#   S7_DIR：指向含 s7.c/s7.h 的目錄（s7 是單檔 Scheme）。預設用 repo vendor 那份
#   （bindings/s7/vendor/，零外部依賴），可覆寫。
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"   # cllm 專案根
: "${S7_DIR:=$HERE/vendor}"

[ -f "$ROOT/build/libcllm.so" ] || { echo "先建 libcllm.so：cmake --build --preset linux-debug" >&2; exit 1; }
[ -f "$S7_DIR/s7.c" ] || { echo "$S7_DIR 下找不到 s7.c" >&2; exit 1; }

# s7.c 當庫一起編（不定 WITH_MAIN）。s7 的 C loader 需要 -ldl；數學要 -lm。
gcc -O2 \
  -I"$S7_DIR" -I"$ROOT/src" \
  "$HERE/host.c" "$HERE/llm_s7.c" "$S7_DIR/s7.c" \
  -o "$HERE/llm-s7" \
  -L"$ROOT/build" -lcllm -Wl,-rpath,"$ROOT/build" -lm -ldl

echo "built $HERE/llm-s7"
echo "試跑：'$HERE/llm-s7' '$HERE/example.scm' \"file://$ROOT/test/fixtures/\""
