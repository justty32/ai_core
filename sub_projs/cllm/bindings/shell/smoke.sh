#!/usr/bin/env bash
# smoke.sh — Shell（llm CLI）綁定離線煙霧測試（跑 example.sh，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

if ! OUT="$(bash "$HERE/example.sh" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] shell: example.sh 執行失敗"; exit 1
fi

fail=0
want() {
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] shell: $2"
  else echo "  [FAIL] shell: $2（缺標記：$1）"; fail=1; fi
}
want "ask => 哼，才不是為你回答的"            "基本 ask"
want "串流 => 哼，才不是為你回答的"           "串流"
want "json(jq) => name=星野 affection=42 lines=2" "schema→JSON→jq"
exit $fail
