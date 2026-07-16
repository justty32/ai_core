#!/usr/bin/env bash
# smoke.sh — C++ 綁定離線煙霧測試（編譯＋跑 example.cpp，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/env.sh；PREFIX 預設 ~/dev）
# 全語言一鍵：../../test/bindings_smoke.sh（本檔是各語言 smoke.sh 的參考實作）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/env.sh"; fi

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if ! g++ -std=c++23 "$HERE/example.cpp" $(pkg-config --cflags --libs cllm) -o "$TMP/example" 2>"$TMP/cc.log"; then
  cat "$TMP/cc.log"; echo "  [FAIL] cpp: example.cpp 編譯失敗"; exit 1
fi
if ! OUT="$("$TMP/example" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] cpp: example 執行失敗"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] cpp: $2"
  else echo "  [FAIL] cpp: $2（缺標記：$1）"; fail=1; fi
}
want "ask => 哼，才不是為你回答的"                                  "基本 ask（聚合）"
want "stream 聚合 => 哼，才不是為你回答的"                          "串流 on_delta＋聚合"
want "ask_as<T> => name=星野 affection=42 lines=2"                  "ask_as<T> 結構化輸出"
want "tool => get_weather(city=東京, unit=celsius)"                 "make_tool<Args>＋args_as<Args>"
want "media out => text=這是語音回覆 media=1 mime=audio/wav bytes=15" "media 輸出（on_media 聚合）"
want "media in+modality => ok"                                      "media 輸入＋modalities 搬運"
want "shell(llm) => 哼，才不是為你回答的"                           "shell-out 呼叫 llm CLI"
exit $fail
