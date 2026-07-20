#!/usr/bin/env bash
# smoke.sh — Janet 綁定離線煙霧測試（跑 example.janet，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 全語言一鍵：../../test/bindings_smoke.sh
# 前置：llm.so 已編好（bash build.sh，或 install-dev.sh 裝在 $PREFIX/lib/janet/）。
#   本檔用「本目錄的 llm.so」+ janet 預設 syspath（spork）跑，不依賴 install。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

[ -f "$HERE/llm.so" ] || bash "$HERE/build.sh" >/dev/null || { echo "  [FAIL] janet: llm.so 建置失敗"; exit 1; }
SYS="$(janet -e '(prin (dyn :syspath))')"   # janet 預設 tree（spork 在此）

if ! OUT="$(JANET_PATH="$HERE:$SYS" janet "$HERE/example.janet" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] janet: example.janet 執行失敗"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] janet: $2"
  else echo "  [FAIL] janet: $2（缺標記：$1）"; fail=1; fi
}
want "ask => 哼，才不是為你回答的"                    "基本 ask（聚合）"
want "串流 => [哼][，才不][是為你][回答的]"           "串流 on-delta＋聚合"
want "json => name=星野 affection=42 lines=2"         "schema+JSON(spork/json) 解析"
want "tool => get_weather(city=東京, unit=celsius)"   "tools：tool_def＋on-tool"
want "media out => mime=audio/wav bytes=15"           "media 輸出（on-media）"
want "media in+modality => ok"                        "media 輸入＋modalities 搬運"
want "shell(llm) => 哼，才不是為你回答的"             "shell-out 呼叫 llm CLI"
exit $fail
