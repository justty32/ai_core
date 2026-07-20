#!/usr/bin/env bash
# smoke.sh — Shell CLI 外殼（cli/llm.sh）離線黑箱煙霧測試（比照 bindings/shell/smoke.sh 風格）。
#
# 驅動 `bash cli/llm.sh` 打 $CLLM_FIXTURES 的 file:// 假回應，grep -F 關鍵標記＋驗退出碼分流，
# 逐條 PASS/FAIL，最後 exit $fail。
# 單獨跑：bash cli/smoke.sh（未 source env.sh 時自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LLM_SH="$HERE/llm.sh"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi
B="$CLLM_FIXTURES"
fail=0

# want_ok <期望 rc> <期望標記> <case 名> -- <llm.sh 參數...>；stdin 一律接 /dev/null（無管線時不卡）。
want_ok() {
  local exp_rc="$1" marker="$2" name="$3"; shift 3; [ "$1" = "--" ] && shift
  local out rc
  out="$(bash "$LLM_SH" "$@" </dev/null 2>&1)"; rc=$?
  if [ "$rc" != "$exp_rc" ]; then
    printf '%s\n' "$out"; echo "  [FAIL] shell-cli: $name（rc=$rc，期望 $exp_rc）"; fail=1; return
  fi
  if [ -n "$marker" ] && ! grep -qF -- "$marker" <<<"$out"; then
    printf '%s\n' "$out"; echo "  [FAIL] shell-cli: $name（缺標記：$marker）"; fail=1; return
  fi
  echo "  [PASS] shell-cli: $name"
}

# 1) 基本 ask
want_ok 0 "哼，才不是為你回答的" "基本 ask" -- 你好 --endpoint "${B}fake/chat/completions"
# 2) --stream（透傳給 llm，逐段吐同一句）
want_ok 0 "哼，才不是為你回答的" "串流 --stream" -- 數到五 --stream --endpoint "${B}fake_stream/chat/completions"
# 3) schema → JSON → jq（結構化輸出經 wrapper 透傳，再用 jq 解）
#    ⚠ C++ `llm` 的 --schema/--tool 收「檔案路徑」（非 core-py 的字面 JSON）；wrapper 透傳，故給路徑。
SCHEMA="$(mktemp)"; printf '{"type":"object"}' > "$SCHEMA"
JSON="$(bash "$LLM_SH" 給我角色 --schema "$SCHEMA" --endpoint "${B}fake_json/chat/completions" </dev/null 2>/dev/null)"
if [ "$(printf '%s' "$JSON" | jq -r .name 2>/dev/null)" = "星野" ]; then
  echo "  [PASS] shell-cli: schema→JSON→jq"
else
  printf '%s\n' "$JSON"; echo "  [FAIL] shell-cli: schema→JSON→jq（name≠星野）"; fail=1
fi
rm -f "$SCHEMA"
# 4) tool（tool_calls 一行一則 JSON，jq 友善）
TOOL="$(mktemp)"; printf '{"name":"get_weather","description":"查天氣","parameters":{"type":"object"}}' > "$TOOL"
want_ok 0 '"tool":"get_weather"' "tool 呼叫" -- 天氣 --tool "$TOOL" --endpoint "${B}fake_tool/chat/completions"
rm -f "$TOOL"
# 5) media 輸入（--image 讀二進位圖檔 + base64，由 llm 做）
PNG="$(mktemp --suffix=.png)"; printf '\x89PNG\r\n\x1a\n' > "$PNG"
want_ok 0 "哼，才不是為你回答的" "media 輸入 --image" -- 描述這張圖 --image "$PNG" --endpoint "${B}fake/chat/completions"
rm -f "$PNG"
# 6) media 輸出（--media-out 落檔，路徑吐 stdout）
MDIR="$(mktemp -d)"
want_ok 0 "$MDIR/llm-media-1" "media 輸出 --media-out" -- 語音 --modality audio --media-out "$MDIR" --endpoint "${B}fake_media/chat/completions"
rm -rf "$MDIR"
# 7) 退出碼：用法錯（未知旗標）→ 1
want_ok 1 "未知旗標" "退出碼 1（用法錯）" -- --bogus 你好
# 8) 退出碼：請求失敗（連不上真 endpoint）→ 2
want_ok 2 "請求失敗" "退出碼 2（請求失敗）" -- 你好 --endpoint "http://127.0.0.1:59999/nope"
# 9) 退出碼：缺 prompt（stdin 也空）→ 1
want_ok 1 "缺少 prompt" "退出碼 1（缺 prompt）" -- --endpoint "${B}fake/chat/completions"
# 10) wrapper 專屬 --meta（自我描述，不呼叫 llm）→ 0
want_ok 0 "llm 二進位＝" "--meta 自我描述" -- --meta
# 11) wrapper 專屬 --meta 也報 config 來源
want_ok 0 "config 來源＝" "--meta 報 config 來源" -- --meta

exit $fail
