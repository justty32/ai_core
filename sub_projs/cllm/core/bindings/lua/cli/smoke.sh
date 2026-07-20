#!/usr/bin/env bash
# smoke.sh — Lua 薄 CLI 外殼的離線煙霧測試（跑 cli/main.lua 打 fixtures，比對關鍵標記＋退出碼）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）。
# 前置：llm.so 已裝在 $PREFIX/lib/lua/5.5/、dkjson 在 $PREFIX/share/lua/（見上層 README）。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi
FX="$CLLM_FIXTURES"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
fail=0

# run <stdin文字> <args...>：跑 CLI，把 stdout+stderr 收進 $OUT、退出碼收進 $RC。
run() { local stdin="$1"; shift
  OUT="$(printf '%s' "$stdin" | LUA_CPATH="$LUA_CPATH_5_5" LUA_PATH="$LUA_PATH_5_5" \
        lua "$HERE/main.lua" "$@" 2>&1)"; RC=$?; }
want() { # want <輸出必含標記> <說明>
  if grep -qF -- "$1" <<<"$OUT"; then echo "  [PASS] lua-cli: $2"
  else echo "  [FAIL] lua-cli: $2（缺標記：$1）"; fail=1; fi; }
code() { # code <期望退出碼> <說明>
  if [ "$RC" = "$1" ]; then echo "  [PASS] lua-cli: $2（退出碼 $1）"
  else echo "  [FAIL] lua-cli: $2（期望退出碼 $1，得 $RC）"; fail=1; fi; }

# ── 能力面（全走離線 fixtures）──
run "" 你好 --endpoint "${FX}fake/chat/completions"
want "哼，才不是為你回答的" "基本 ask"; code 0 "基本 ask"

run "" 數到五 --stream --endpoint "${FX}fake_stream/chat/completions"
want "哼，才不是為你回答的" "--stream 串流聚合"; code 0 "--stream"

run "" 給我角色 --schema '{"type":"object"}' --endpoint "${FX}fake_json/chat/completions"
want '"name":"星野"' "schema → 結構化 JSON"; code 0 "schema"

run "" 東京天氣 --endpoint "${FX}fake_tool/chat/completions" \
    --tool '{"name":"get_weather","description":"查天氣","parameters":{"type":"object","properties":{"city":{"type":"string"},"unit":{"type":"string"}}}}'
want '"tool":"get_weather"' "tools：tool_call 一行 JSON"
want '"city":"東京"'        "tool_call arguments（dkjson 解出）"; code 0 "tool"

run "" 描述這張圖 --media 'data:image/png;base64,iVBORw0KGgo=' \
    --modality 'audio={"voice":"alloy"}' --endpoint "${FX}fake/chat/completions"
want "哼，才不是為你回答的" "media 輸入（URL 直通）＋modalities 搬運"; code 0 "media in + modality"

printf '{"mime":"image/png","data":"iVBORw0KGgo="}' > "$TMP/desc.json"
run "" 描述 --media "$TMP/desc.json" --endpoint "${FX}fake/chat/completions"
want "哼，才不是為你回答的" "media 描述子（.json → base64 解碼直通）"; code 0 "media descriptor"

MD="$TMP/out"; mkdir -p "$MD"
run "" 說句話 --media-out "$MD" --endpoint "${FX}fake_media/chat/completions"
want "llm-media-1.wav" "media 輸出（--media-out 落檔並印路徑）"; code 0 "media out"
[ -s "$MD/llm-media-1.wav" ] && echo "  [PASS] lua-cli: media 落檔非空" \
  || { echo "  [FAIL] lua-cli: media 落檔缺/空"; fail=1; }

# ── config 三層來源 ──
printf '{"endpoint":"%sfake/chat/completions","temperature":0.7}' "$FX" > "$TMP/conf.json"
run "" 你好 --config "$TMP/conf.json"
want "哼，才不是為你回答的" "config 檔提供 endpoint"; code 0 "config 檔"
run "" 你好 --config "$TMP/conf.json" --endpoint "${FX}fake_json/chat/completions"
want '"name":"星野"' "命令列旗標覆寫 config"; code 0 "旗標覆寫 config"

# ── stdin 合體 ──
run "PIPED_DATA" 前 - 後 --endpoint "${FX}fake/chat/completions"
want "哼，才不是為你回答的" "「-」stdin 插入點合體"; code 0 "stdin 合體"

# ── 退出碼分流 ──
run "" --help;                                    code 0 "--help"
run "" 你好 --bogus;                              code 1 "未知旗標 → 1"
run "" 你好 --image;                              code 1 "旗標缺值 → 1"
run "" --endpoint "${FX}fake/chat/completions";   code 1 "缺 prompt → 1"
run "" 你好 --schema '{bad';                       code 1 "--schema 非法 JSON → 1"
run "" 你好 --tool '{"description":"x"}';          code 1 "--tool 缺 name/parameters → 1"
run "" 你好 --config /no/such.json;               code 1 "--config 讀不到 → 1"
run "" 你好 --media-out /no/such/dir;             code 1 "--media-out 非目錄 → 1"
run "" 你好 --temperature abc --endpoint "${FX}fake/chat/completions"; code 1 "數值型別錯 → 1"
run "" 你好 --endpoint "http://127.0.0.1:59999/v1/chat/completions" --timeout-ms 800
code 2 "連不上真 endpoint → 2"

exit $fail
