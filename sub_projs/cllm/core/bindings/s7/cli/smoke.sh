#!/usr/bin/env bash
# smoke.sh — s7 薄 CLI 外殼的離線煙霧測試（跑 cli/main.scm，比對關鍵標記＋退出碼）。
# 單獨跑：bash cli/smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 參考風格：../smoke.sh（binding 煙測）＋ core-py/test（用 fixtures 驗 CLI 行為與退出碼）。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"                 # …/bindings/s7/cli
MAIN="$HERE/main.scm"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi
command -v llm-s7 >/dev/null 2>&1 || { echo "  [FAIL] s7-cli: PATH 找不到 llm-s7（先 source env.sh）"; exit 1; }
command -v jq     >/dev/null 2>&1 || { echo "  [FAIL] s7-cli: 需要 jq（binding 靠 jq 剖 JSON）"; exit 1; }

EP() { printf '%s%s' "$CLLM_FIXTURES" "$1"; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
fail=0

# want <期望退出碼> <輸出必含標記> <說明> -- <CLI 參數...>
want() {
  local exp="$1" mark="$2" desc="$3"; shift 3; [ "$1" = "--" ] && shift
  local out ec
  out="$("$@" 2>&1)"; ec=$?
  if [ "$ec" -ne "$exp" ]; then
    echo "  [FAIL] s7-cli: $desc（退出碼 $ec≠$exp）"; fail=1; return
  fi
  if [ -n "$mark" ] && ! grep -qF -- "$mark" <<<"$out"; then
    echo "  [FAIL] s7-cli: $desc（缺標記：$mark）"; fail=1; return
  fi
  echo "  [PASS] s7-cli: $desc"
}

# ── 能力面（對齊 llm CLI；全走離線 fixtures）──
want 0 "哼，才不是為你回答的" "基本 ask" -- \
  llm-s7 "$MAIN" 你好 --endpoint "$(EP fake/chat/completions)"
want 0 "哼，才不是為你回答的" "--stream 串流逐段" -- \
  llm-s7 "$MAIN" --stream 數到五 --endpoint "$(EP fake_stream/chat/completions)"
want 0 '"name":' "schema→結構化 JSON" -- \
  llm-s7 "$MAIN" 給我角色 --schema '{"type":"object"}' --endpoint "$(EP fake_json/chat/completions)"
# schema 出來的 JSON 可經 jq 抽欄位
NAME="$(llm-s7 "$MAIN" 給我角色 --schema '{"type":"object"}' --endpoint "$(EP fake_json/chat/completions)" | jq -r .name)"
if [ "$NAME" = "星野" ]; then echo "  [PASS] s7-cli: schema JSON 可 jq 抽 name=星野"
else echo "  [FAIL] s7-cli: schema JSON jq 抽欄位（得 '$NAME'）"; fail=1; fi
want 0 '{"tool":"get_weather","id":"call_abc123","arguments":{"city":"東京"' "tool_calls 一行一則 JSON" -- \
  llm-s7 "$MAIN" 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object"}}' \
    --endpoint "$(EP fake_tool/chat/completions)"

# media 輸入（data URI 直通）＋ modality 搬運不炸
want 0 "哼，才不是為你回答的" "media 輸入(data URI)+modality" -- \
  llm-s7 "$MAIN" 描述這張圖 --media "data:image/png;base64,iVBORw0KGgo=" \
    --modality 'audio={"voice":"alloy"}' --endpoint "$(EP fake/chat/completions)"

# media 輸出落檔（--media-out）：檔案該落地
llm-s7 "$MAIN" 說句話 --media-out "$TMP" --endpoint "$(EP fake_media/chat/completions)" >/dev/null 2>&1
if [ -f "$TMP/llm-media-1.wav" ]; then echo "  [PASS] s7-cli: media 輸出落檔 llm-media-1.wav"
else echo "  [FAIL] s7-cli: media 輸出落檔（沒生成 llm-media-1.wav）"; fail=1; fi

# stdin × 位置參數合體
OUT="$(printf '報告內文' | llm-s7 "$MAIN" 總結這份 --endpoint "$(EP fake/chat/completions)" 2>&1)"
if grep -qF "哼，才不是為你回答的" <<<"$OUT"; then echo "  [PASS] s7-cli: stdin×位置參數合體"
else echo "  [FAIL] s7-cli: stdin×位置參數合體"; fail=1; fi

# ── 退出碼分流 ──
want 1 "缺少 prompt" "缺 prompt→用法錯(1)" -- \
  llm-s7 "$MAIN" --endpoint "$(EP fake/chat/completions)" </dev/null
want 1 "未知旗標" "未知旗標→用法錯(1)" -- \
  llm-s7 "$MAIN" --bogus </dev/null
want 1 "不是合法 JSON" "--schema 壞 JSON→用法錯(1)" -- \
  llm-s7 "$MAIN" hi --schema 'not json' --endpoint "$(EP fake/chat/completions)"
want 2 "請求失敗" "連不上真 endpoint→請求失敗(2)" -- \
  llm-s7 "$MAIN" 你好 --endpoint "http://127.0.0.1:1/v1/chat/completions"

exit $fail
