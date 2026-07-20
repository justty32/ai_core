#!/usr/bin/env bash
# smoke.sh — Go 版 llm CLI（薄外殼，重用 bindings/go 的 cllm.Ask）離線煙霧測試。
# 走 core/test/fixtures 的假回應（file://），驗：基本 ask／--stream／schema→JSON／tool／media 輸入輸出／
# 退出碼分流（用法錯→1、請求失敗→2）。單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）。
# 風格比照 ../smoke.sh：跑 CLI、grep -F 關鍵標記、逐條 PASS/FAIL、最後 exit $fail。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GODIR="$(cd "$HERE/.." && pwd)"   # bindings/go：module 根（package cllm）＋ ./cli
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

# 用「編出的 binary」而非 go run——go run 會把非零退出碼一律壓成 1，測不到 2/130 分流。
BIN="$(mktemp -d)/llm-go"
if ! go build -C "$GODIR" -o "$BIN" ./cli 2>"$BIN.log"; then
  cat "$BIN.log"; echo "  [FAIL] go-cli: build 失敗"; exit 1
fi
EP() { printf '%s%s/chat/completions' "$CLLM_FIXTURES" "$1"; }
MO="$(mktemp -d)"
trap 'rm -rf "$(dirname "$BIN")" "$MO"' EXIT

fail=0
want() { # want <輸出> <必含標記> <說明>
  if grep -qF -- "$2" <<<"$1"; then echo "  [PASS] go-cli: $3"
  else echo "  [FAIL] go-cli: $3（缺標記：$2）"; fail=1; fi
}
code() { # code <實際退出碼> <期望> <說明>
  if [ "$1" = "$2" ]; then echo "  [PASS] go-cli: $3（退出碼 $1）"
  else echo "  [FAIL] go-cli: $3（退出碼 $1，期望 $2）"; fail=1; fi
}

# ① 基本 ask
OUT="$("$BIN" 你好 --endpoint "$(EP fake)" </dev/null 2>&1)"
want "$OUT" "哼，才不是為你回答的" "基本 ask（聚合吐 stdout）"
# ② --stream
OUT="$("$BIN" --stream 數到五 --endpoint "$(EP fake_stream)" </dev/null 2>&1)"
want "$OUT" "哼，才不是為你回答的" "串流 --stream（逐段吐 stdout）"
# ③ schema → JSON
OUT="$("$BIN" 給我角色 --schema '{"type":"object"}' --endpoint "$(EP fake_json)" </dev/null 2>&1)"
want "$OUT" '"name":"星野"' "schema 結構化輸出（JSON 直吐）"
# ④ tool＋on_tool（一行一則 JSON）
OUT="$("$BIN" 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object"}}' --endpoint "$(EP fake_tool)" </dev/null 2>&1)"
want "$OUT" '{"tool":"get_weather","id":"call_abc123","arguments":{"city":"東京","unit":"celsius"}}' "tool_calls（一行一則 JSON）"
# ⑤ media 輸入＋modalities（搬運不炸；fake 忽略 body 回固定文字）
OUT="$("$BIN" 描述這張圖 --image 'data:image/png;base64,iVBORw0KGgo=' --modality 'audio={"voice":"alloy"}' --endpoint "$(EP fake)" </dev/null 2>&1)"
want "$OUT" "哼，才不是為你回答的" "media 輸入（--image data:）＋modality 搬運"
# ⑥ media 輸出（落檔 --media-out，路徑吐 stdout）
OUT="$("$BIN" 說句話 --media-out "$MO" --endpoint "$(EP fake_media)" </dev/null 2>&1)"
want "$OUT" "llm-media-1.wav" "media 輸出（落檔 --media-out）"
[ -f "$MO/llm-media-1.wav" ] && want "found" "found" "產出媒體落到檔案" || { echo "  [FAIL] go-cli: 產出媒體未落檔"; fail=1; }
# ⑦ stdin × 位置參數合體（「-」插入點）
OUT="$(printf '報告內文' | "$BIN" 總結 - --endpoint "$(EP fake)" 2>&1)"
want "$OUT" "哼，才不是為你回答的" "stdin 合體（「-」插入點）"

# ── 退出碼分流 ──
"$BIN" 你好 --endpoint "$(EP fake)" </dev/null >/dev/null 2>&1;                    code "$?" 0 "成功→0"
"$BIN" --bogus 你好 </dev/null >/dev/null 2>&1;                                    code "$?" 1 "未知旗標→1"
"$BIN" </dev/null >/dev/null 2>&1;                                                 code "$?" 1 "缺 prompt→1"
"$BIN" 嗨 --schema 'not json' --endpoint "$(EP fake)" </dev/null >/dev/null 2>&1;  code "$?" 1 "壞 --schema JSON→1"
"$BIN" 嗨 --temperature abc --endpoint "$(EP fake)" </dev/null >/dev/null 2>&1;    code "$?" 1 "數值旗標型別錯→1"
"$BIN" 你好 --endpoint 'http://127.0.0.1:1/chat/completions' --timeout-ms 800 </dev/null >/dev/null 2>&1; code "$?" 2 "連不上真 endpoint→2"

exit $fail
