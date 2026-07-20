#!/usr/bin/env bash
# run.sh — 跑 play.janet。預設打離線 fixture；要打真後端改 play 檔裡的 endpoint。
#   (import llm) 走 env.sh 設好的 JANET_PATH（裝好的 lib/janet/llm.so ＋ janet 預設 tree 的 spork）。
set -euo pipefail
cd "$(dirname "$0")"
[ -z "${CLLM_FIXTURES:-}" ] && . ~/dev/cllm/env.sh
BASE="${1:-$CLLM_FIXTURES}"
janet play.janet "$BASE"
