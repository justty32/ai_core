#!/usr/bin/env bash
# smoke.sh — Python binding CLI（cli/main.py）離線煙霧測試。
# 跑真 CLI（over ctypes binding llm.py → libcllm.so），用 file:// 離線 fixtures，逐條 PASS/FAIL。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 感覺基準＝../smoke.sh（binding 煙測）＋ core-py/test（示範用 fixtures 驗 CLI 行為與退出碼）。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

FX="$CLLM_FIXTURES"
CLI="python3 $HERE/main.py"
fail=0

# want_out <標記> <說明> -- <CLI 參數...>：跑 CLI（stdin 給 EOF），退出碼須 0 且 stdout 含標記。
want_out() {
  local needle="$1" desc="$2"; shift 3  # 吃掉 needle desc 與 '--'
  local out; out="$($CLI "$@" </dev/null 2>/dev/null)"; local ec=$?
  if [ "$ec" = 0 ] && grep -qF -- "$needle" <<<"$out"; then echo "  [PASS] $desc"
  else echo "  [FAIL] $desc（exit=$ec 缺標記：$needle｜out=<$out>）"; fail=1; fi
}

# want_exit <期望碼> <說明> -- <CLI 參數...>：只驗退出碼（stdin 可經 WANT_STDIN 環境變數帶入）。
want_exit() {
  local want="$1" desc="$2"; shift 3
  local ec; printf '%s' "${WANT_STDIN:-}" | $CLI "$@" >/dev/null 2>&1; ec=$?
  if [ "$ec" = "$want" ]; then echo "  [PASS] exit=$ec  $desc"
  else echo "  [FAIL] exit=$ec（期望 $want） $desc"; fail=1; fi
}

echo "== 能力面（離線 fixtures）=="
want_out "才不是為你回答的" "基本 ask（聚合）" -- 你好 --endpoint "${FX}fake/chat/completions"
want_out "才不是為你回答的" "--stream 串流" -- 數到五 --endpoint "${FX}fake_stream/chat/completions" --stream
want_out '"name":"星野"'   "schema→結構化 JSON" -- 給我角色 --endpoint "${FX}fake_json/chat/completions" --schema '{"type":"object"}'
want_out '"tool":"get_weather"' "tool：tool_def＋tool_call 一行 JSON" -- 天氣 --endpoint "${FX}fake_tool/chat/completions" \
  --tool '{"name":"get_weather","description":"查天氣","parameters":"{\"type\":\"object\"}"}'
want_out "才不是為你回答的" "media 輸入＋modality 搬運不炸" -- 描述 --endpoint "${FX}fake/chat/completions" \
  --media 'data:image/png;base64,iVBORw0KGgo=' --modality 'audio={"voice":"alloy"}'

# media 輸出：落檔到暫存目錄、路徑吐 stdout
MO="$(mktemp -d)"
want_out "llm-media-1.wav" "media 輸出（--media-out 落檔）" -- 說句話 --endpoint "${FX}fake_media/chat/completions" --media-out "$MO"
[ -f "$MO/llm-media-1.wav" ] && echo "  [PASS] media 檔確實落地" || { echo "  [FAIL] media 檔沒落地"; fail=1; }
rm -rf "$MO"

# config 檔：無 --endpoint，靠 config 提供
CONF="$(mktemp)"; printf '{"endpoint":"%sfake/chat/completions"}' "$FX" >"$CONF"
want_out "才不是為你回答的" "--config 提供 endpoint" -- 你好 --config "$CONF"
LLM_CLI_CONFIG="$CONF" want_out "才不是為你回答的" "env LLM_CLI_CONFIG 指路" -- 你好
rm -f "$CONF"

echo "== 退出碼分流 =="
want_exit 0 "成功" -- 你好 --endpoint "${FX}fake/chat/completions"
want_exit 0 "--help" -- --help
want_exit 1 "用法錯：未知旗標" -- 你好 --bogus
want_exit 1 "用法錯：旗標缺值（--image）" -- 你好 --image
want_exit 1 "用法錯：缺 prompt（stdin 空）" -- --endpoint "${FX}fake/chat/completions"
want_exit 1 "用法錯：--config 讀不到" -- 你好 --config /no/such.json
want_exit 1 "用法錯：--schema 非法 JSON" -- 你好 --schema '{bad json'
want_exit 1 "用法錯：--tool 缺 name/parameters" -- 你好 --tool '{"description":"x"}'
want_exit 1 "用法錯：--media-out 不是目錄" -- 你好 --media-out /no/such/dir
want_exit 1 "用法錯：數值旗標型別錯" -- 你好 --temperature abc --endpoint "${FX}fake/chat/completions"
want_exit 2 "請求失敗：連不上真 endpoint" -- 你好 --endpoint "http://127.0.0.1:59999/v1/chat/completions" --timeout-ms 800

# stdin×位置參數合體：「-」＝stdin 插入點
WANT_STDIN="世界" want_exit 0 "stdin 合體（「-」插入點）" -- 你好 - --endpoint "${FX}fake/chat/completions"

echo "== 結果：$([ $fail = 0 ] && echo 全綠 || echo 有失敗) =="
exit $fail
