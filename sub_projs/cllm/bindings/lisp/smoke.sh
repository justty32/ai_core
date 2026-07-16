#!/usr/bin/env bash
# smoke.sh — Common Lisp 綁定離線煙霧測試（跑 example.lisp，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/env.sh；PREFIX 預設 ~/dev）
# 全語言一鍵：../../test/bindings_smoke.sh（參考實作見 bindings/cpp/smoke.sh）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/env.sh"; fi

if ! OUT="$(sbcl --script "$HERE/example.lisp" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] lisp: example.lisp 執行失敗"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] lisp: $2"
  else echo "  [FAIL] lisp: $2（缺標記：$1）"; fail=1; fi
}
want "[cl] ask => 哼，才不是為你回答的"                     "基本 ask"
want "[cl] 串流 => 哼，才不是為你回答的"                    "串流 on-delta＋聚合"
want "[cl] json => name=星野 affection=42"                  "schema → JSON（shasht 解析）"
want "[cl] tool => get_weather(city=東京, unit=celsius)"    ":tools＋:on-tool（工具呼叫）"
want "[cl] media out => mime=audio/wav bytes=15"            ":on-media（產出媒體聚合）"
want "[cl] media in+modality => ok"                         ":media＋:modalities 搬運"
want "[cl] shell(llm) => 哼，才不是為你回答的"               "shell-out 呼叫 llm CLI"
exit $fail
