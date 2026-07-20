#!/usr/bin/env bash
# smoke.sh — Janet 薄 CLI 外殼的離線煙霧測試（跑 cli/main.janet，比對 stdout／body／退出碼）。
# 單獨跑：bash cli/smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 走離線 fixtures（$CLLM_FIXTURES），不連網。真正做事的是 binding 的 llm/ask，本檔只驗 CLI 接線。
#   body 類 case 靠 libcllm 的 LLM_DUMP_BODY=1（binding 直接繼承）把請求 JSON 印到 stderr 來比對。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CLI="$HERE/main.janet"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi
fx() { printf '%sfake%s/chat/completions' "$CLLM_FIXTURES" "${1:+_$1}"; }  # fx→fake、fx json→fake_json

fail=0
pass_msg() { echo "  [PASS] janet-cli: $1"; }
fail_msg() { echo "  [FAIL] janet-cli: $1"; fail=1; }

# want_out <標記> <說明> <argv...>：跑 CLI（stdin 空），驗退出碼 0 且 stdout 含標記
want_out() { local m="$1" d="$2"; shift 2
  local out; out="$(janet "$CLI" "$@" </dev/null 2>/dev/null)"; local rc=$?
  if [ $rc -eq 0 ] && grep -qF -- "$m" <<<"$out"; then pass_msg "$d"; else fail_msg "$d（rc=$rc out=<$out>）"; fi; }

# want_body <標記> <說明> <argv...>：LLM_DUMP_BODY=1 跑，驗退出碼 0 且 stderr（請求 body）含標記
want_body() { local m="$1" d="$2"; shift 2
  local err; err="$(LLM_DUMP_BODY=1 janet "$CLI" "$@" </dev/null 2>&1 1>/dev/null)"; local rc=$?
  if [ $rc -eq 0 ] && grep -qF -- "$m" <<<"$err"; then pass_msg "$d"; else fail_msg "$d（rc=$rc body=<$err>）"; fi; }

# want_exit <期望碼> <說明> <argv...>：驗退出碼分流（stdin 空）
want_exit() { local w="$1" d="$2"; shift 2
  janet "$CLI" "$@" </dev/null >/dev/null 2>&1; local rc=$?
  if [ "$rc" -eq "$w" ]; then pass_msg "exit=$rc $d"; else fail_msg "exit=$rc（期望 $w）$d"; fi; }

# want_pipe <標記> <說明> <stdin> <argv...>：從 stdin 餵入，驗 stdout 含標記
want_pipe() { local m="$1" d="$2" s="$3"; shift 3
  local out; out="$(printf '%s' "$s" | janet "$CLI" "$@" 2>/dev/null)"; local rc=$?
  if [ $rc -eq 0 ] && grep -qF -- "$m" <<<"$out"; then pass_msg "$d"; else fail_msg "$d（rc=$rc out=<$out>）"; fi; }
want_pipe_body() { local m="$1" d="$2" s="$3"; shift 3
  local err; err="$(printf '%s' "$s" | LLM_DUMP_BODY=1 janet "$CLI" "$@" 2>&1 1>/dev/null)"; local rc=$?
  if [ $rc -eq 0 ] && grep -qF -- "$m" <<<"$err"; then pass_msg "$d"; else fail_msg "$d（rc=$rc body=<$err>）"; fi; }

echo "-- 輸出正確 --"
want_out "才不是為你回答的" "位置參數當 prompt（非串流）" 你好 --endpoint "$(fx)"
want_out "才不是為你回答的" "--stream 串流逐段" 數到五 --stream --endpoint "$(fx stream)"
want_out "才不是為你回答的" "-- 分隔符（旗標在前 -- prompt）" --stream --endpoint "$(fx stream)" -- 你好
want_out '"name":"星野"' "--schema 收字面 JSON → 結構化輸出" 給我角色 --schema '{"type":"object"}' --endpoint "$(fx json)"
want_pipe "才不是為你回答的" "沒位置參數 → 讀 stdin" $'從管線\n' --endpoint "$(fx)"
want_pipe "才不是為你回答的" "prompt＋stdin 合體" $'附加資料\n' 總結這份 --endpoint "$(fx)"
want_pipe "才不是為你回答的" "「-」＝stdin 插入點" $'內容\n' 把 - 翻成英文 --endpoint "$(fx)"

echo "-- system / tools / modalities（body 或 stdout）--"
want_body '"role":"system","content":"你是傲嬌貓"' "--system → body 首則 system 訊息" --system 你是傲嬌貓 你好 --endpoint "$(fx)"
want_out '"tool":"get_weather"' "--tool → tool_calls 一行 JSON 吐 stdout" 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object","properties":{"city":{"type":"string"}}}}' --endpoint "$(fx tool)"
want_out '"city":"東京"' "--tool → arguments 原樣內嵌（保留 UTF-8）" 東京天氣 --tool '{"name":"get_weather","parameters":{"type":"object"}}' --endpoint "$(fx tool)"

echo "-- media 輸入直通三分流（body dump）--"
want_body '"url":"data:image/png;base64,BBBB"' "--media data: URL 直通（不編碼）" 描述這張圖 --media 'data:image/png;base64,BBBB' --endpoint "$(fx)"
want_body '"url":"data:image/png;base64,CCCC"' "--image 別名亦支援 URL 直通" 描述這張圖 --image 'data:image/png;base64,CCCC' --endpoint "$(fx)"
MU="$(mktemp --suffix=.json)"; printf '{"url":"data:image/png;base64,AAAA"}' > "$MU"
want_body '"url":"data:image/png;base64,AAAA"' "--media .json 描述子 {url} → 直通" 描述這張圖 --media "$MU" --endpoint "$(fx)"; rm -f "$MU"
MD="$(mktemp --suffix=.json)"; printf '{"mime":"image/png","data":"aGVsbG8="}' > "$MD"
want_body 'data:image/png;base64,aGVsbG8=' "--media .json 描述子 {mime,data} → 直通" 描述這張圖 --media "$MD" --endpoint "$(fx)"; rm -f "$MD"

echo "-- media 輸出落檔 --"
OUTD="$(mktemp -d)"
want_out "llm-media-1.wav" "--modality＋--media-out → 媒體落檔、路徑吐 stdout" 說你好 --modality 'audio={"voice":"alloy"}' --media-out "$OUTD" --endpoint "$(fx media)"
if [ -f "$OUTD/llm-media-1.wav" ] && [ "$(cat "$OUTD/llm-media-1.wav")" = "hello-wav-bytes" ]; then
  pass_msg "落檔內容正確（base64 已解碼）"; else fail_msg "落檔內容不對"; fi
rm -rf "$OUTD"
want_exit 0 "媒體但沒給 --media-out → 明說丟棄、不炸" 說你好 --endpoint "$(fx media)"

echo "-- 設定來源 --"
CONF="$(mktemp)"; printf '{"endpoint":"%s","temperature":0.7}' "$(fx)" > "$CONF"
want_out "才不是為你回答的" "--config 提供 endpoint（無 --endpoint 旗標）" 你好 --config "$CONF"
env_out="$(LLM_CLI_CONFIG="$CONF" janet "$CLI" 你好 </dev/null 2>/dev/null)"
if grep -qF "才不是為你回答的" <<<"$env_out"; then pass_msg "env LLM_CLI_CONFIG 指路"; else fail_msg "env LLM_CLI_CONFIG 指路"; fi
want_out '"name":"星野"' "命令列旗標覆寫 config 的 endpoint" 你好 --config "$CONF" --endpoint "$(fx json)"
rm -f "$CONF"

echo "-- 退出碼分流 --"
want_exit 0 "成功" 你好 --endpoint "$(fx)"
want_exit 0 "--help" --help
want_exit 1 "未知旗標" 你好 --bogus
want_exit 1 "旗標缺值（--image）" 你好 --image
want_exit 1 "缺 prompt（stdin 空）" --endpoint "$(fx)"
want_exit 1 "「-」但 stdin 空（拼完仍空）" - --endpoint "$(fx)"
want_exit 1 "--config 讀不到" 你好 --config /no/such.json
want_exit 1 "--schema 給路徑（當字面、非法 JSON）" 你好 --schema /no/such.json
want_exit 1 "--schema 字面非法 JSON" 你好 --schema '{bad json'
want_exit 1 "--system 缺值" 你好 --system
want_exit 1 "--tool 給路徑（當字面、非法 JSON）" 你好 --tool /no/such.json
want_exit 1 "--tool 字面缺 name/parameters" 你好 --tool '{"description":"x"}'
want_exit 1 "--modality config 字面非法 JSON" 你好 --modality 'audio=/no/such.json'
want_exit 1 "--media .json 描述子讀不到" 你好 --media /no/such.json
want_exit 1 "--media-out 不是目錄" 你好 --media-out /no/such/dir
want_exit 1 "數值旗標型別錯（--temperature abc）" 你好 --temperature abc --endpoint "$(fx)"
CB="$(mktemp)"; printf '{bad json' > "$CB"; want_exit 1 "config JSON 壞" 你好 --config "$CB"; rm -f "$CB"
MB="$(mktemp --suffix=.json)"; printf '{"foo":1}' > "$MB"; want_exit 1 "--media .json 描述子形狀不符" 你好 --media "$MB"; rm -f "$MB"
want_exit 2 "請求失敗（連不上真 endpoint）" 你好 --endpoint http://127.0.0.1:59999/v1/chat/completions --timeout-ms 800

exit $fail
