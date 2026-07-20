#!/usr/bin/env bash
# smoke.sh — C 薄 CLI 外殼（llm-c）離線煙霧測試：編譯 → 打 file:// fixtures → 逐條 grep 關鍵標記。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 對齊 core-py/test/ 與 bindings/c/smoke.sh 風格：驗基本 ask／--stream／schema／tool／media
# 輸入輸出／stdin 合體／config 三層／退出碼 0-1-2 分流。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi
B="$CLLM_FIXTURES"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
BIN="$TMP/llm-c"
if ! cc "$HERE"/[a-z]*.c $(pkg-config --cflags --libs cllm jansson) -o "$BIN" 2>"$TMP/cc.log"; then
  cat "$TMP/cc.log"; echo "  [FAIL] c-cli: 編譯失敗"; exit 1
fi

fail=0
# want <說明> <期望標記> <期望退出碼> -- <llm-c 參數...>；stdin 一律導 /dev/null（避免卡在 tty）
want() {
  local desc="$1" mark="$2" want_ec="$3"; shift 3; [ "$1" = "--" ] && shift
  local out ec
  out="$("$BIN" "$@" </dev/null 2>&1)"; ec=$?
  if [ "$ec" != "$want_ec" ]; then echo "  [FAIL] c-cli: $desc（退出碼 $ec≠$want_ec）"; fail=1; return; fi
  if [ -n "$mark" ] && ! grep -qF -- "$mark" <<<"$out"; then
    echo "  [FAIL] c-cli: $desc（缺標記：$mark）"; fail=1; return; fi
  echo "  [PASS] c-cli: $desc"
}

want "基本 ask（退出碼 0）"        "哼，才不是為你回答的"                 0 -- 你好 --endpoint "${B}fake/chat/completions"
want "--stream 逐段"              "哼，才不是為你回答的"                 0 -- 數到五 --stream --endpoint "${B}fake_stream/chat/completions"
want "schema → JSON"             '{"name":"星野","affection":42'        0 -- 給我角色 --schema '{"type":"object"}' --endpoint "${B}fake_json/chat/completions"
want "tool → 一行 JSON"          '"tool":"get_weather"'                 0 -- 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object"}}' --endpoint "${B}fake_tool/chat/completions"
want "media 輸入（data URI 直通）" "哼，才不是為你回答的"                 0 -- 描述這張圖 --media 'data:image/png;base64,iVBORw0KGgo=' --endpoint "${B}fake/chat/completions"

# media 輸出：落檔目錄 + 驗內容
MOUT="$TMP/mout"; mkdir -p "$MOUT"
want "media 輸出（落檔路徑）"      "llm-media-1.wav"                      0 -- 說句話 --media-out "$MOUT" --endpoint "${B}fake_media/chat/completions"
if grep -qF "hello-wav-bytes" "$MOUT/llm-media-1.wav" 2>/dev/null; then echo "  [PASS] c-cli: media 落檔內容"; else echo "  [FAIL] c-cli: media 落檔內容"; fail=1; fi

# config 三層：config 檔供 endpoint
CFG="$TMP/cfg.json"; printf '{"endpoint":"%sfake/chat/completions","temperature":0.7}\n' "$B" > "$CFG"
want "config 檔供 endpoint"        "哼，才不是為你回答的"                 0 -- 你好 --config "$CFG"

# 退出碼分流
want "用法錯：未知旗標（1）"        "未知旗標"                             1 -- 你好 --nope
want "用法錯：缺 prompt（1）"       "缺少 prompt"                          1 --
want "用法錯：數值型別錯（1）"      ""                                     1 -- 你好 --temperature abc --endpoint "${B}fake/chat/completions"
want "用法錯：config 讀不到（1）"    ""                                     1 -- 你好 --config /no/such.json
want "請求失敗：連不上（2）"        ""                                     2 -- 你好 --endpoint "file:///no/such/fixture"
want "--help（0）"                 "用法：llm-c"                          0 -- --help

# stdin 合體（需真 stdin，另跑）
if [ "$(printf '數到五' | "$BIN" --endpoint "${B}fake/chat/completions")" = "哼，才不是為你回答的……下不為例喔。" ]; then
  echo "  [PASS] c-cli: 純 stdin 當 prompt"; else echo "  [FAIL] c-cli: 純 stdin 當 prompt"; fail=1; fi

exit $fail
