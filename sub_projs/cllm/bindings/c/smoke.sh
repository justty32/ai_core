#!/usr/bin/env bash
# smoke.sh — C 綁定離線煙霧測試（編譯＋跑 example.c，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/env.sh；PREFIX 預設 ~/dev）
# 全語言一鍵：../../test/bindings_smoke.sh（參考實作見 ../cpp/smoke.sh）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/env.sh"; fi

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if ! cc "$HERE/example.c" $(pkg-config --cflags --libs cllm jansson) -o "$TMP/example" 2>"$TMP/cc.log"; then
  cat "$TMP/cc.log"; echo "  [FAIL] c: example.c 編譯失敗"; exit 1
fi
if ! OUT="$("$TMP/example" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] c: example 執行失敗"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] c: $2"
  else echo "  [FAIL] c: $2（缺標記：$1）"; fail=1; fi
}
want "ask => 哼，才不是為你回答的"                    "基本 ask（聚合）"
want "json => name=星野 affection=42 lines=2"         "schema＋jansson 解析"
want "tool => get_weather(city=東京, unit=celsius)"   "tools：llm_tool_def_t＋on_tool"
want "media out => mime=audio/wav bytes=15"           "media 輸出（on_media）"
want "media in+modality => ok"                        "media 輸入＋modalities 搬運"
want "shell(llm) => 哼，才不是為你回答的"             "shell-out 呼叫 llm CLI"
exit $fail
