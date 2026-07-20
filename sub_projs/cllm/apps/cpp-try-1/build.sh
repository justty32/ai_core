#!/usr/bin/env bash
# build.sh —— 編 cpp-try-1（走 pkg-config，與 cllm-lab/cpp/run.sh 同一套慣例）。
#
# cllm binding 標頭在 ~/dev/include/cllm/，libcllm.so 靠 .pc 帶的 rpath 找；只要 pkg-config
# 找得到 cllm.pc（PKG_CONFIG_PATH 由 ~/dev/cllm/env.sh 設）即可。
#
# 用：
#   source ~/dev/cllm/env.sh   # 或自己 export PKG_CONFIG_PATH=~/dev/lib/pkgconfig
#   ./build.sh                 # → build/cpp-try-1
#   ./build.sh test            # 另外編 + 跑離線單元測試（test/test_classify）
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

# 沒 source env.sh 時補上（讓 pkg-config 找得到 cllm.pc）——存在才 source，不強求。
if ! pkg-config --exists cllm 2>/dev/null; then
  [ -f "$HOME/dev/cllm/env.sh" ] && . "$HOME/dev/cllm/env.sh"
fi
if ! pkg-config --exists cllm 2>/dev/null; then
  echo "✗ pkg-config 找不到 cllm —— 先 source ~/dev/cllm/env.sh 或設 PKG_CONFIG_PATH" >&2
  exit 1
fi

CXX="${CXX:-g++}"
mkdir -p build

echo "· 編 app：$CXX -std=c++23 src/main.cpp → build/cpp-try-1"
$CXX -std=c++23 -O2 -Isrc src/main.cpp $(pkg-config --cflags --libs cllm) -o build/cpp-try-1
echo "· 完成 → build/cpp-try-1"

if [ "${1:-}" = "test" ]; then
  echo "· 編測試：build/test_classify"
  $CXX -std=c++23 -Isrc test/test_classify.cpp $(pkg-config --cflags --libs cllm) -o build/test_classify
  echo "· 跑離線單元測試（不觸網）："
  ./build/test_classify
fi
