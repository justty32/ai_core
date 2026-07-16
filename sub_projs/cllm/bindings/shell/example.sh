#!/usr/bin/env bash
# example.sh — 純用 shell 呼叫 cllm CLI（llm）＋ jq 解析 JSON。
# 跑：source ~/dev/env.sh 後  bash example.sh ["$CLLM_FIXTURES"]
set -euo pipefail
BASE="${1:-${CLLM_FIXTURES:-}}"

echo "[sh] ask => $(llm 你好 --endpoint "${BASE}fake/chat/completions")"

printf '[sh] 串流 => '; llm 數到五 --stream --endpoint "${BASE}fake_stream/chat/completions"; echo

# schema → JSON → jq
SCHEMA="$(mktemp)"; printf '{"type":"object"}' > "$SCHEMA"
RAW="$(llm 給我角色 --schema "$SCHEMA" --endpoint "${BASE}fake_json/chat/completions")"
echo "[sh] json(jq) => name=$(printf '%s' "$RAW" | jq -r .name) affection=$(printf '%s' "$RAW" | jq -r .affection) lines=$(printf '%s' "$RAW" | jq '.lines|length')"
rm -f "$SCHEMA"
