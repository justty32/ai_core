#!/usr/bin/env bash
# smoke.sh — s7 綁定離線煙霧測試（重編 llm-s7 ＋跑 example.scm，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/env.sh；PREFIX 預設 ~/dev）
# 參考實作：../cpp/smoke.sh（本檔照它的模式寫，前綴 s7:）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/env.sh"; fi
# s7 原始碼用 repo vendor 那份（bindings/s7/vendor/，零外部依賴）；S7_DIR 可覆寫。
: "${S7_DIR:=$HERE/vendor}"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

[ -f "$S7_DIR/s7.c" ] || { echo "  [FAIL] s7: \$S7_DIR($S7_DIR) 下找不到 s7.c"; exit 1; }

if ! gcc -O2 -I"$S7_DIR" $(pkg-config --cflags cllm) \
     "$HERE/host.c" "$HERE/llm_s7.c" "$S7_DIR/s7.c" \
     $(pkg-config --libs cllm) -lm -ldl -o "$TMP/llm-s7" 2>"$TMP/cc.log"; then
  cat "$TMP/cc.log"; echo "  [FAIL] s7: llm_s7.c 編譯失敗"; exit 1
fi

if ! OUT="$("$TMP/llm-s7" "$HERE/example.scm" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] s7: example.scm 執行失敗"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] s7: $2"
  else echo "  [FAIL] s7: $2（缺標記：$1）"; fail=1; fi
}
want "ask => 哼，才不是為你回答的"                                  "基本 ask（聚合）"
want "串流 => [哼][，才不][是為你][回答的]"                         "串流 on-delta"
want "tool => get_weather"                                          ":tools/:on-tool 收到工具呼叫"
want '{"city":"東京","unit":"celsius"}'                             ":on-tool arguments 原字串"
want "(city=東京)"                                                  ":on-tool arguments 可經 jq 抽欄位"
want "media out => mime=audio/wav bytes=15"                         "fake_media + :on-media 收到媒體輸出"
want "json(jq) => name=星野"                                        "schema+JSON(jq)"
want "media in+modality => ok"                                      ":media(data URI)+:modalities 搬運"
want "shell(llm) => 哼，才不是為你回答的"                           "shell-out 呼叫 llm CLI"
exit $fail
