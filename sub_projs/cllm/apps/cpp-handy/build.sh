#!/usr/bin/env bash
# build.sh —— 編 cpp-handy 四工具（llme / zhtw / wf / mail）成獨立執行檔。
#
# 純 shell-out 應用：不 link libcllm（真正的「手」在 runtime 轉呼外部 llm/claude/sibling），
# 故不需 pkg-config cllm、不需 cllm 開發環境。只要一支 C++20/23 編譯器即可。
#
# 用：
#   ./build.sh                 # → bin/llme bin/zhtw bin/wf bin/mail
#   CXX=clang++ ./build.sh     # 換編譯器（本機 g++ 16 / clang++ 22 皆可）
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

CXX="${CXX:-g++}"
STD="${STD:-c++23}"
mkdir -p bin

for tool in llme zhtw wf mail; do
  echo "· 編 $tool：$CXX -std=$STD src/$tool.cpp → bin/$tool"
  "$CXX" -std="$STD" -O2 -Isrc "src/$tool.cpp" -o "bin/$tool"
done

echo "· 完成 → bin/{llme,zhtw,wf,mail}"
