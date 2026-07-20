#!/usr/bin/env bash
# smoke.sh — C++ 薄 CLI 外殼（llm-cpp）離線煙霧測試：編譯＋跑 CLI＋驗關鍵標記／退出碼。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）
# 走 file:// 假回應（$CLLM_FIXTURES），不連網、與 C++ 原生 llm 黑箱可比。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

FX="${CLLM_FIXTURES%/}" # file://…/fixtures（去尾斜線）
fx() { echo "$FX/$1/chat/completions"; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
BIN="$TMP/llm-cpp"   # 建到 temp（比照 c/go），不留 in-tree 產物
# ── 編譯（cli/*.cpp）──
if ! g++ -std=c++23 "$HERE"/*.cpp $(pkg-config --cflags --libs cllm) -o "$BIN" 2>"$TMP/cc.log"; then
  cat "$TMP/cc.log"; echo "  [FAIL] cpp-cli: 編譯失敗"; exit 1
fi

fail=0
pass() { echo "  [PASS] cpp-cli: $1"; }
bad()  { echo "  [FAIL] cpp-cli: $1"; fail=1; }

# want_out <說明> <輸出必含標記> -- <argv...>（stdin 取自 /dev/null）
want_out() {
  local desc="$1" marker="$2"; shift 2; [ "$1" = "--" ] && shift
  local out; out="$("$BIN" "$@" </dev/null 2>/dev/null)"
  if grep -qF -- "$marker" <<<"$out"; then pass "$desc"
  else bad "$desc（缺標記：$marker；得：$out）"; fi
}
# want_exit <說明> <期望碼> -- <argv...>（stdin 可用 WANT_STDIN 傳）
want_exit() {
  local desc="$1" code="$2"; shift 2; [ "$1" = "--" ] && shift
  printf '%s' "${WANT_STDIN:-}" | "$BIN" "$@" >/dev/null 2>&1
  local rc=$?
  if [ "$rc" -eq "$code" ]; then pass "$desc"
  else bad "$desc（期望退出碼 $code，得 $rc）"; fi
}

echo "-- 能力面 --"
want_out "基本 ask"                "才不是為你回答的"    -- 你好 --endpoint "$(fx fake)"
want_out "--stream 逐段＋聚合"      "才不是為你回答的"    -- 數到五 --stream --endpoint "$(fx fake_stream)"
want_out "schema→結構化 JSON"      '"name":"星野"'       -- 給我角色 --schema '{"type":"object"}' --endpoint "$(fx fake_json)"
want_out "tool_calls 一行一則 JSON" 'get_weather'        -- 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object","properties":{"city":{"type":"string"}}}}' --endpoint "$(fx fake_tool)"
want_out "tool 參數解析（東京）"    '東京'                -- 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object"}}' --endpoint "$(fx fake_tool)"
want_out "media 輸入（data: URL 直通）" "才不是為你回答的" -- 描述這張圖 --media 'data:image/png;base64,iVBORw0KGgo=' --endpoint "$(fx fake)"
want_out "modality 搬運不炸"        "才不是為你回答的"    -- 說你好 --modality 'audio={"voice":"alloy"}' --endpoint "$(fx fake)"

# media 輸出：落檔目錄 → 印路徑＋檔案存在
MDIR="$TMP/mout"; mkdir -p "$MDIR"
mout="$("$BIN" 說句話 --media-out "$MDIR" --endpoint "$(fx fake_media)" </dev/null 2>/dev/null)"
if grep -qF -- "llm-media-1.wav" <<<"$mout" && [ -f "$MDIR/llm-media-1.wav" ]; then
  pass "media 輸出（落檔＋印路徑）"
else bad "media 輸出（缺 llm-media-1.wav；得：$mout）"; fi

# media 輸入：.json 描述子直通
DESC="$TMP/desc.json"; printf '{"url":"data:image/png;base64,iVBORw0KGgo="}' > "$DESC"
want_out "media .json 描述子直通"   "才不是為你回答的"    -- 描述 --media "$DESC" --endpoint "$(fx fake)"

echo "-- 設定來源 --"
CONF="$TMP/conf.json"; printf '{"endpoint":"%s","temperature":0.7}' "$(fx fake)" > "$CONF"
want_out "--config 提供 endpoint"   "才不是為你回答的"    -- 你好 --config "$CONF"
LLM_CLI_CONFIG="$CONF" want_out "env LLM_CLI_CONFIG 指路" "才不是為你回答的" -- 你好
want_out "旗標覆寫 config 的 endpoint" '"name":"星野"'    -- 你好 --config "$CONF" --endpoint "$(fx fake_json)"

echo "-- 退出碼分流 --"
want_exit "成功（0）"               0 -- 你好 --endpoint "$(fx fake)"
want_exit "--help（0）"             0 -- --help
want_exit "未知旗標（1）"           1 -- 你好 --bogus --endpoint "$(fx fake)"
want_exit "旗標缺值 --image（1）"   1 -- 你好 --image
WANT_STDIN="" want_exit "缺 prompt（1）" 1 -- --endpoint "$(fx fake)"
want_exit "--config 讀不到（1）"    1 -- 你好 --config /no/such.json
want_exit "--schema 字面非法 JSON（1）" 1 -- 你好 --schema '{bad'
want_exit "--tool 缺 name/parameters（1）" 1 -- 你好 --tool '{"description":"x"}'
want_exit "--modality config 非法 JSON（1）" 1 -- 你好 --modality 'audio={bad'
want_exit "--media .json 描述子讀不到（1）" 1 -- 你好 --media /no/such.json
want_exit "--media-out 不是目錄（1）" 1 -- 你好 --media-out /no/such/dir
want_exit "數值旗標型別錯（1）"      1 -- 你好 --temperature abc --endpoint "$(fx fake)"
want_exit "請求失敗連不上（2）"      2 -- 你好 --endpoint http://127.0.0.1:59999/v1/chat/completions --timeout-ms 800

exit $fail
