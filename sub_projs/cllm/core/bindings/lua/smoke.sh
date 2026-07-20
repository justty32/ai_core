#!/usr/bin/env bash
# smoke.sh — Lua 綁定離線煙霧測試（跑 example.lua，比對關鍵標記）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 全語言一鍵：../../test/bindings_smoke.sh（參考實作見 ../cpp/smoke.sh）
# 前置：llm.so 已編好裝在 $PREFIX/lib/lua/5.5/（見本檔上層 README「重編」段落）。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

if ! OUT="$(LUA_CPATH="$LUA_CPATH_5_5" LUA_PATH="$LUA_PATH_5_5" lua "$HERE/example.lua" "$CLLM_FIXTURES" 2>&1)"; then
  printf '%s\n' "$OUT"; echo "  [FAIL] lua: example.lua 執行失敗"; exit 1
fi

fail=0
want() { # want <輸出必含的標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] lua: $2"
  else echo "  [FAIL] lua: $2（缺標記：$1）"; fail=1; fi
}
want "ask => 哼，才不是為你回答的"                    "基本 ask（聚合）"
want "串流 => [哼][，才不][是為你][回答的]"           "串流 on_delta＋聚合"
want "json => name=星野 affection=42 lines=2"         "schema+JSON(dkjson) 解析"
want "tool => get_weather(city=東京, unit=celsius)"   "tools：tool_def＋on_tool"
want "media out => mime=audio/wav bytes=15"           "media 輸出（on_media）"
want "media in+modality => ok"                        "media 輸入＋modalities 搬運"
want "shell(llm) => 哼，才不是為你回答的"             "shell-out 呼叫 llm CLI"
exit $fail
