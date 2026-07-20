#!/usr/bin/env bash
# vendor.sh —— 把 cllm tools 的「build 產物」帶進 app（INTEGRATION.md 推薦的 vendored sidecar）。
#
# 這兩支是 C++ 二進位（不是純 stdlib script），所以 vendored 的是「編好的產物」而非原始碼：
# 從 $CLLM_ROOT/build/tools 複製 anthropic-proxy + llm-login + liblogin.so 進 vendor/cllm-tools/，
# 之後 up.sh 就能離線、與 repo 解耦地起 sidecar。上游更新時重跑本腳本即可。
#
# 用：CLLM_ROOT=~/repo/ai_core/sub_projs/cllm ./scripts/vendor.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLLM_ROOT="${CLLM_ROOT:-$HOME/repo/ai_core/sub_projs/cllm}"
SRC="$CLLM_ROOT/build/tools"
DST="$HERE/vendor/cllm-tools"

if [ ! -d "$SRC" ]; then
  echo "找不到 build 產物：$SRC" >&2
  echo "先建：cd $CLLM_ROOT && cmake --preset linux-debug && cmake --build --preset linux-debug" >&2
  exit 1
fi

mkdir -p "$DST"
for f in anthropic-proxy llm-login liblogin.so; do
  if [ ! -e "$SRC/$f" ]; then echo "缺產物 $SRC/$f —— 先 build 全部 tools" >&2; exit 1; fi
  cp -f "$SRC/$f" "$DST/$f"
  echo "vendored: $f"
done

# 版本標記：記來源與時間，方便日後對上游
{
  echo "source=$SRC"
  echo "vendored_at=$(date -Iseconds)"
  echo "cllm_head=$(git -C "$CLLM_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
} > "$DST/VENDOR_INFO"

echo "完成 → $DST"
