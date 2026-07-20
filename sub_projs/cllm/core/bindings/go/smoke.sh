#!/usr/bin/env bash
# smoke.sh — Go 綁定（cgo）離線煙霧測試（go run example＋比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 全語言一鍵：../../test/bindings_smoke.sh（參考實作見 ../cpp/smoke.sh）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

if ! OUT="$(cd "$HERE" && go run ./example "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] go: example 執行失敗（go run）"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] go: $2"
  else echo "  [FAIL] go: $2（缺標記：$1）"; fail=1; fi
}
want "[go] ask => 哼，才不是為你回答的"                    "基本 ask（聚合）"
want "[go] 串流 => [哼][，才不][是為你][回答的]"           "串流 OnDelta"
want "[go] json => name=星野 affection=42 lines=2"         "schema＋encoding/json"
want "[go] tool => get_weather(city=東京, unit=celsius)"   "Tools＋OnTool（tool_call）"
want "[go] media out => mime=audio/wav bytes=15"           "media 輸出（OnMedia）"
want "[go] media in+modality => ok"                        "media 輸入（MediaIn）＋Modalities 搬運"
want "[go] shell(llm) => 哼，才不是為你回答的"              "shell-out 呼叫 llm CLI"
exit $fail
