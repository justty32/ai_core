#!/usr/bin/env bash
# smoke.sh — Fennel 版 llm CLI（本資料夾）離線煙霧測試：驅動 ./cllm 打 file:// 假回應，
#   逐條 grep -F 關鍵標記／驗退出碼分流，PASS/FAIL 後 exit $fail。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 前置：Fennel 直接用 lua 的 llm.so（見 ../../lua/llm.c），跑在 lua 5.4；離線 fixtures＝$CLLM_FIXTURES。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi
CLLM="$HERE/cllm"
EP="${CLLM_FIXTURES}fake/chat/completions"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

fail=0
ok()  { echo "  [PASS] fennel-cli: $1"; }
no()  { echo "  [FAIL] fennel-cli: $1（$2）"; fail=1; }
want()   { grep -qF -- "$2" <<<"$OUT" && ok "$1" || no "$1" "缺標記：$2"; }
wantrc() { [ "$RC" = "$2" ] && ok "$1" || no "$1" "退出碼＝$RC，期望 $2"; }

# ── 能力面（stdout only）──
OUT="$("$CLLM" 你好 --endpoint "$EP" </dev/null 2>/dev/null)"; RC=$?
want "基本 ask" "哼，才不是為你回答的"; wantrc "基本 ask 退出碼 0" 0

OUT="$("$CLLM" --stream 數到五 --endpoint "${CLLM_FIXTURES}fake_stream/chat/completions" </dev/null 2>/dev/null)"; RC=$?
want "串流 --stream（聚合吐 stdout）" "哼，才不是為你回答的"; wantrc "串流退出碼 0" 0

OUT="$("$CLLM" 給我角色 --schema '{"type":"object"}' --endpoint "${CLLM_FIXTURES}fake_json/chat/completions" </dev/null 2>/dev/null)"; RC=$?
want "schema→JSON 結構化輸出" '"name":"星野"'; wantrc "schema 退出碼 0" 0

OUT="$("$CLLM" 東京天氣如何？ --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object","properties":{"city":{"type":"string"},"unit":{"type":"string"}}}}' --endpoint "${CLLM_FIXTURES}fake_tool/chat/completions" </dev/null 2>/dev/null)"; RC=$?
want "tool_calls 一行一則 JSON" '"tool":"get_weather"'
want "tool_calls arguments 內嵌" '"city":"東京"'; wantrc "tool 退出碼 0" 0

OUT="$("$CLLM" 說句話 --media-out "$TMP" --endpoint "${CLLM_FIXTURES}fake_media/chat/completions" </dev/null 2>/dev/null)"; RC=$?
want "media 輸出落檔（路徑吐 stdout）" "llm-media-1.wav"; wantrc "media 輸出退出碼 0" 0
[ "$(wc -c < "$TMP/llm-media-1.wav" 2>/dev/null)" = 15 ] && ok "media 落檔內容 15 bytes" || no "media 落檔內容 15 bytes" "位元數不符"

OUT="$("$CLLM" 描述這張圖 --media 'data:image/png;base64,iVBORw0KGgo=' --modality 'audio={"voice":"alloy"}' --endpoint "$EP" </dev/null 2>/dev/null)"; RC=$?
want "media 輸入（data URL）＋modality 搬運" "哼，才不是為你回答的"; wantrc "media 輸入退出碼 0" 0

# 二進位圖檔路徑（三分流之三：讀檔＋內核 base64）
printf '\x89PNG\r\n\x1a\nFAKE' > "$TMP/pic.png"
OUT="$("$CLLM" 描述 --media "$TMP/pic.png" --endpoint "$EP" </dev/null 2>/dev/null)"; RC=$?
want "media 輸入（二進位圖檔讀檔編碼）" "哼，才不是為你回答的"; wantrc "media 二進位退出碼 0" 0

# .json 描述子（三分流之二：mime+data base64 直通）
printf '{"mime":"image/png","data":"aGVsbG8="}' > "$TMP/desc.json"
OUT="$("$CLLM" 描述 --media "$TMP/desc.json" --endpoint "$EP" </dev/null 2>/dev/null)"; RC=$?
want "media 輸入（.json 描述子直通）" "哼，才不是為你回答的"; wantrc "media 描述子退出碼 0" 0

# ── prompt × stdin 合體 ──
OUT="$(printf '一些資料\n' | "$CLLM" 總結這份 --endpoint "$EP" 2>/dev/null)"; RC=$?
want "prompt＋stdin 合體" "哼，才不是為你回答的"; wantrc "stdin 合體退出碼 0" 0

OUT="$(printf 'DIFF內容\n' | "$CLLM" 把 - 寫成訊息 --endpoint "$EP" 2>/dev/null)"; RC=$?
want "「-」stdin 插入點" "哼，才不是為你回答的"; wantrc "「-」退出碼 0" 0

# ── config 三層 ──
printf '{"endpoint":"%sfake/chat/completions","temperature":0.5}' "$CLLM_FIXTURES" > "$TMP/cfg.json"
OUT="$("$CLLM" 你好 --config "$TMP/cfg.json" </dev/null 2>/dev/null)"; RC=$?
want "config 檔設 endpoint" "哼，才不是為你回答的"; wantrc "config 檔退出碼 0" 0
OUT="$(LLM_CLI_CONFIG="$TMP/cfg.json" "$CLLM" 你好 </dev/null 2>/dev/null)"; RC=$?
want "LLM_CLI_CONFIG env 指定 config" "哼，才不是為你回答的"; wantrc "env config 退出碼 0" 0

# ── 退出碼分流（stderr 併入觀測）──
OUT="$("$CLLM" 你好 --nope </dev/null 2>&1)"; RC=$?
wantrc "用法錯：未知旗標→1" 1
OUT="$("$CLLM" --endpoint "$EP" </dev/null 2>&1)"; RC=$?
wantrc "用法錯：缺 prompt→1" 1
OUT="$("$CLLM" 你好 --temperature abc --endpoint "$EP" </dev/null 2>&1)"; RC=$?
wantrc "用法錯：數值型別錯→1" 1
OUT="$("$CLLM" 你好 --schema 'not json' --endpoint "$EP" </dev/null 2>&1)"; RC=$?
wantrc "用法錯：schema 壞 JSON→1" 1
OUT="$("$CLLM" 你好 --endpoint "${CLLM_FIXTURES}NOPE/chat/completions" </dev/null 2>&1)"; RC=$?
wantrc "請求失敗：連不上 endpoint→2" 2
"$CLLM" --help </dev/null >/dev/null 2>&1; RC=$?
wantrc "--help→0" 0

exit $fail
