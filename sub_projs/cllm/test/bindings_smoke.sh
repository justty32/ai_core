#!/usr/bin/env bash
# bindings_smoke.sh — 全語言綁定一鍵離線煙霧測試：輪流呼叫 bindings/<lang>/smoke.sh。
# 前置：bash install-dev.sh（常駐前綴；PREFIX 可覆寫，預設 ~/dev）。
# 跑：bash test/bindings_smoke.sh   （單語言：bash bindings/<lang>/smoke.sh）
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/dev}"
if [ ! -f "$PREFIX/cllm/env.sh" ]; then
  echo "找不到 $PREFIX/cllm/env.sh —— 先跑：bash install-dev.sh"; exit 2
fi
. "$PREFIX/cllm/env.sh"

LANGS=(c cpp lua fennel s7 python lisp go shell)
pass=0; fail=0; skip=0
for lang in "${LANGS[@]}"; do
  s="$ROOT/bindings/$lang/smoke.sh"
  if [ ! -f "$s" ]; then
    echo "[SKIP] $lang（尚無 smoke.sh）"; skip=$((skip + 1)); continue
  fi
  echo "== $lang =="
  if bash "$s"; then
    echo "[PASS] $lang"; pass=$((pass + 1))
  else
    echo "[FAIL] $lang"; fail=$((fail + 1))
  fi
done
echo ""
echo "== 結果：PASS=$pass FAIL=$fail SKIP=$skip =="
[ "$fail" -eq 0 ]
