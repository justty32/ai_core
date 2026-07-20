#!/usr/bin/env bash
# run.sh — 編譯（如需要）＋跑 play.*。預設打離線 fixture；要打真後端改 play 檔裡的 endpoint。
set -euo pipefail
cd "$(dirname "$0")"
[ -z "${CLLM_FIXTURES:-}" ] && . ~/dev/cllm/env.sh
BASE="${1:-$CLLM_FIXTURES}"
go run . "$BASE"
