#!/usr/bin/env bash
# smoke.sh — Common Lisp 薄 CLI 外殼的離線煙霧測試（跑 cli/main.lisp，比對關鍵標記＋退出碼）。
# 單獨跑：bash smoke.sh（自動 source $PREFIX/cllm/env.sh；PREFIX 預設 ~/dev）。
# 走離線 fixtures（$CLLM_FIXTURES，file://），不連網；真正做事的是 binding 的 cllm:ask。
# 風格比照 ../smoke.sh：逐條 PASS/FAIL，最後 exit $fail。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${CLLM_FIXTURES:-}" ]; then . "${PREFIX:-$HOME/dev}/cllm/env.sh"; fi

CLI="$HERE/main.lisp"
FX="$CLLM_FIXTURES"                          # 以 / 結尾，故直接拼 <name>/chat/completions
run() { sbcl --script "$CLI" "$@" </dev/null 2>/dev/null; }        # stdin 從 /dev/null，避免卡住
run_err() { sbcl --script "$CLI" "$@" </dev/null 2>&1; }           # 合併 stderr（驗 dump body）

fail=0
# want <關鍵標記> <說明> -- <CLI 參數...>：跑 CLI，stdout 需含標記（退出碼 0）。
want() {
  local needle="$1" desc="$2"; shift 3
  local out; out="$(run "$@")"
  if grep -qF -- "$needle" <<<"$out"; then echo "  [PASS] lisp-cli: $desc"
  else echo "  [FAIL] lisp-cli: $desc（缺標記：$needle）"; fail=1; fi
}
# want_body <關鍵標記> <說明> -- <CLI 參數...>：LLM_DUMP_BODY=1 跑，驗印到 stderr 的請求 JSON 含標記。
want_body() {
  local needle="$1" desc="$2"; shift 3
  local out; out="$(LLM_DUMP_BODY=1 run_err "$@")"
  if grep -qF -- "$needle" <<<"$out"; then echo "  [PASS] lisp-cli: $desc"
  else echo "  [FAIL] lisp-cli: $desc（body 缺標記：$needle）"; fail=1; fi
}
# want_exit <期望碼> <說明> -- <CLI 參數...>：驗退出碼分流。
want_exit() {
  local want="$1" desc="$2"; shift 3
  run "$@" >/dev/null 2>&1; local code=$?
  if [ "$code" = "$want" ]; then echo "  [PASS] lisp-cli: exit=$code  $desc"
  else echo "  [FAIL] lisp-cli: exit=$code（期望 $want）  $desc"; fail=1; fi
}

TOOL='{"name":"get_weather","description":"查天氣","parameters":{"type":"object","properties":{"city":{"type":"string"}}}}'
MEDIA_OUT="$(mktemp -d)"
MBIN="$(mktemp --suffix=.json)"; printf '{"mime":"image/png","data":"aGVsbG8="}' > "$MBIN"
MBAD="$(mktemp --suffix=.json)"; printf '{"foo":1}' > "$MBAD"

echo "-- 能力面（輸出／body 正確）--"
want "才不是為你回答的"  "基本 ask（位置參數當 prompt）"                 -- 你好 --endpoint "${FX}fake/chat/completions"
want "才不是為你回答的"  "--stream 串流逐段"                            -- 數到五 --stream --endpoint "${FX}fake_stream/chat/completions"
want '"name":"星野"'    "--schema 收字面 JSON → 結構化輸出"             -- 給我角色 --schema '{"type":"object"}' --endpoint "${FX}fake_json/chat/completions"
want '"tool":"get_weather"' "--tool → tool_calls 一行 JSON 吐 stdout"  -- 東京天氣 --tool "$TOOL" --endpoint "${FX}fake_tool/chat/completions"
want '"city":"東京"'    "--tool → arguments 原樣內嵌"                   -- 東京天氣 --tool "$TOOL" --endpoint "${FX}fake_tool/chat/completions"
want "llm-media-1.wav"  "--media-out → 產出媒體落檔、路徑吐 stdout"     -- 說你好 --modality 'audio={"voice":"alloy"}' --media-out "$MEDIA_OUT" --endpoint "${FX}fake_media/chat/completions"
want_body '"role":"system","content":"你是傲嬌貓"' "--system → body 首則 system 訊息" -- --system 你是傲嬌貓 你好 --endpoint "${FX}fake/chat/completions"
want_body 'data:image/png;base64,BBBB'   "--media data: URL 直通（不編碼）"        -- 描述這張圖 --media 'data:image/png;base64,BBBB' --endpoint "${FX}fake/chat/completions"
want_body 'data:image/png;base64,aGVsbG8=' "--media .json 描述子 {mime,data} 直通" -- 描述這張圖 --media "$MBIN" --endpoint "${FX}fake/chat/completions"

echo "-- 落檔內容 --"
if [ "$(cat "$MEDIA_OUT/llm-media-1.wav" 2>/dev/null)" = "hello-wav-bytes" ]; then
  echo "  [PASS] lisp-cli: 媒體落檔內容正確（base64 已解碼）"
else echo "  [FAIL] lisp-cli: 媒體落檔內容不對"; fail=1; fi

echo "-- prompt × stdin 合體 --"
if grep -qF "才不是為你回答的" <<<"$(printf '從管線\n' | sbcl --script "$CLI" --endpoint "${FX}fake/chat/completions" 2>/dev/null)"; then
  echo "  [PASS] lisp-cli: 沒位置參數 → 讀 stdin"
else echo "  [FAIL] lisp-cli: 沒位置參數 → 讀 stdin"; fail=1; fi
if grep -qF "才不是為你回答的" <<<"$(printf '內容\n' | sbcl --script "$CLI" 把 - 翻成英文 --endpoint "${FX}fake/chat/completions" 2>/dev/null)"; then
  echo "  [PASS] lisp-cli: 「-」＝stdin 插入點"
else echo "  [FAIL] lisp-cli: 「-」＝stdin 插入點"; fail=1; fi

echo "-- 設定來源 --"
CONF="$(mktemp --suffix=.json)"; printf '{"endpoint":"%sfake/chat/completions","temperature":0.7}' "$FX" > "$CONF"
want "才不是為你回答的"  "--config 提供 endpoint（無 --endpoint 旗標）"  -- 你好 --config "$CONF"
want '"name":"星野"'    "命令列旗標覆寫 config 的 endpoint"             -- 你好 --config "$CONF" --endpoint "${FX}fake_json/chat/completions"

echo "-- 退出碼分流 --"
want_exit 0 "成功"                               -- 你好 --endpoint "${FX}fake/chat/completions"
want_exit 0 "--help"                             -- --help
want_exit 1 "未知旗標"                            -- 你好 --bogus
want_exit 1 "旗標缺值（--image）"                 -- 你好 --image
want_exit 1 "缺 prompt（stdin 空）"              -- --endpoint "${FX}fake/chat/completions"
want_exit 1 "--config 讀不到"                     -- 你好 --config /no/such.json
want_exit 1 "--schema 字面非法 JSON"              -- 你好 --schema '{bad json'
want_exit 1 "--tool 缺 name/parameters"          -- 你好 --tool '{"description":"x"}'
want_exit 1 "--media .json 描述子形狀不符"        -- 你好 --media "$MBAD"
want_exit 1 "數值旗標型別錯（--temperature abc）"  -- 你好 --temperature abc --endpoint "${FX}fake/chat/completions"
want_exit 2 "請求失敗（連不上真 endpoint）"        -- 你好 --endpoint 'http://127.0.0.1:59999/v1/chat/completions' --timeout-ms 800

rm -rf "$MEDIA_OUT" "$MBIN" "$MBAD" "$CONF"
echo "== lisp-cli 結果：$([ $fail = 0 ] && echo 全綠 || echo 有 FAIL) =="
exit $fail
