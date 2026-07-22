#!/usr/bin/env bash
# cli_smoke.sh — cllm `llm` CLI 的離線黑箱煙霧測試。
#
# 為什麼是 shell 腳本：CLI 是個執行檔，最誠實的測法就是「真的去跑它、驗 stdout／退出碼」，
#   而非在同一個行程內單元測。全程走 file:// 假回應（test/fixtures/），不連網、不吃真後端。
#   ⚠ 離線 fixture 驗得到「解析／組裝／退出碼分流」，驗不到「後端錯誤語意」（假回應永遠 200、
#     欄位齊全）——那是真後端的活，try_4 那條線扛（見 README「接真後端」）。
#
# 用：先建好 build/llm，再 `bash test/cli_smoke.sh`（或給它執行權後直接跑）。全過回 0，任一敗回 1。

set -u

# 定位：腳本在 test/ 底下 → 專案根＝上一層。binary 預設 build/llm，可用 $LLM 覆寫。
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BIN="${LLM:-$ROOT/build/llm}"
# file:// fixture 的 root：Windows(MSYS/Git Bash) 下 pwd 給的是 MSYS 路徑（/c/…），
# native llm.exe 的 fopen 開不了；pwd -W 轉成真 Windows 磁碟路徑（C:/…）。Linux 不變。
FXROOT="$ROOT"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) FXROOT="$(cd "$HERE/.." && pwd -W)";; esac
FX="file://$FXROOT/test/fixtures"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$BIN" ]; then
    echo "找不到可執行的 llm：$BIN（先 cmake --build --preset linux-debug）" >&2
    exit 1
fi

pass=0
fail=0

# expect_exit <期望碼> <說明> -- <指令...>：跑指令、比對退出碼。
expect_exit() {
    local want="$1" desc="$2"; shift 2
    [ "$1" = "--" ] && shift
    "$@" >/dev/null 2>&1
    local got=$?
    if [ "$got" = "$want" ]; then
        printf '  [PASS] exit=%s  %s\n' "$got" "$desc"; pass=$((pass+1))
    else
        printf '  [FAIL] exit=%s（期望 %s）  %s\n' "$got" "$want" "$desc" >&2; fail=$((fail+1))
    fi
}

# expect_out <期望子字串> <說明> -- <指令...>：跑指令、驗 stdout 含子字串且退出碼 0。
expect_out() {
    local needle="$1" desc="$2"; shift 2
    [ "$1" = "--" ] && shift
    local out; out="$("$@" 2>/dev/null)"; local got=$?
    if [ "$got" = 0 ] && printf '%s' "$out" | grep -qF -- "$needle"; then
        printf '  [PASS] %s\n' "$desc"; pass=$((pass+1))
    else
        printf '  [FAIL] exit=%s out=<%s>  %s\n' "$got" "$out" "$desc" >&2; fail=$((fail+1))
    fi
}

# expect_body <期望子字串> <說明> -- <指令...>：LLM_DUMP_BODY=1 跑指令、驗印到 stderr 的請求 JSON
#   含子字串且退出碼 0（用來黑箱檢查組出去的 body——file:// 假回應不看 body，只能靠 dump 觀察）。
expect_body() {
    local needle="$1" desc="$2"; shift 2
    [ "$1" = "--" ] && shift
    local err; err="$(LLM_DUMP_BODY=1 "$@" 2>&1 >/dev/null)"; local got=$?
    if [ "$got" = 0 ] && printf '%s' "$err" | grep -qF -- "$needle"; then
        printf '  [PASS] %s\n' "$desc"; pass=$((pass+1))
    else
        printf '  [FAIL] exit=%s body=<%s>  %s\n' "$got" "$err" "$desc" >&2; fail=$((fail+1))
    fi
}

# expect_err <期望子字串> <說明> -- <指令...>：跑指令、驗 stderr 含子字串且退出碼 0（診斷面輸出，如 --usage）。
expect_err() {
    local needle="$1" desc="$2"; shift 2
    [ "$1" = "--" ] && shift
    local err; err="$("$@" 2>&1 >/dev/null)"; local got=$?
    if [ "$got" = 0 ] && printf '%s' "$err" | grep -qF -- "$needle"; then
        printf '  [PASS] %s\n' "$desc"; pass=$((pass+1))
    else
        printf '  [FAIL] exit=%s err=<%s>  %s\n' "$got" "$err" "$desc" >&2; fail=$((fail+1))
    fi
}

echo "== llm CLI 離線煙霧測試 =="
echo "binary: $BIN"

# ── (1) happy path：輸出正確 ──
echo "-- 輸出正確 --"
expect_out "才不是為你回答的" "位置參數當 prompt（非串流）" \
    -- "$BIN" 你好 --endpoint "$FX/fake/chat/completions"
expect_out "才不是為你回答的" "--stream 串流逐段" \
    -- "$BIN" 數到五 --stream --endpoint "$FX/fake_stream/chat/completions"
expect_out "才不是為你回答的" "沒位置參數 → 讀 stdin" \
    -- sh -c "echo 從管線 | '$BIN' --endpoint '$FX/fake/chat/completions'"
expect_out "才不是為你回答的" "prompt＋stdin 合體（指令在前、資料在後）" \
    -- sh -c "echo 附加資料 | '$BIN' 總結這份 --endpoint '$FX/fake/chat/completions'"
expect_out "才不是為你回答的" "「-」＝stdin 插入點" \
    -- sh -c "echo 內容 | '$BIN' 把 - 翻成英文 --endpoint '$FX/fake/chat/completions'"
expect_out "才不是為你回答的" "旗標在前 + prompt（--stream 你好）" \
    -- "$BIN" --stream --endpoint "$FX/fake_stream/chat/completions" 你好
expect_out "才不是為你回答的" "-- 分隔符（旗標在前 -- prompt）" \
    -- "$BIN" --stream --endpoint "$FX/fake_stream/chat/completions" -- 你好
printf '{"type":"object"}' > "$TMP/sc.json"
expect_out '"name":"星野"' "--schema 結構化輸出" \
    -- "$BIN" 給我角色 --schema "$TMP/sc.json" --endpoint "$FX/fake_json/chat/completions"

# ── (1d) --usage：token 用量吐 stderr；串流時 body 多送 stream_options（不給旗標＝完全不送）──
echo "-- usage --"
expect_err "用量：prompt=42 completion=21 total=63" "--usage → 用量吐 stderr（非串流）" \
    -- "$BIN" 你好 --usage --endpoint "$FX/fake/chat/completions"
expect_err "用量：prompt=8 completion=4 total=12" "--usage → 用量吐 stderr（串流末塊）" \
    -- "$BIN" 你好 --usage --stream --endpoint "$FX/fake_stream/chat/completions"
expect_out "才不是為你回答的" "--usage 不汙染 stdout（答案照常）" \
    -- "$BIN" 你好 --usage --endpoint "$FX/fake/chat/completions"
expect_body '"stream_options":{"include_usage":true}' "--usage＋--stream → body 送 stream_options" \
    -- "$BIN" 你好 --usage --stream --endpoint "$FX/fake_stream/chat/completions"
if LLM_DUMP_BODY=1 "$BIN" 你好 --stream --endpoint "$FX/fake_stream/chat/completions" 2>&1 >/dev/null \
    | grep -qF '"stream_options"'; then
    printf '  [FAIL] 未給 --usage 卻送了 stream_options\n' >&2; fail=$((fail+1))
else
    printf '  [PASS] 未給 --usage → body 無 stream_options\n'; pass=$((pass+1))
fi

# ── (1c) --system：在 user 前插一則真 system role 訊息（驗組出去的 body）──
echo "-- system role --"
expect_body '"role":"system","content":"你是傲嬌貓"' "--system → body 首則為 system 訊息" \
    -- "$BIN" --system 你是傲嬌貓 你好 --endpoint "$FX/fake/chat/completions"
expect_body '"role":"user"' "--system 後仍保留 user 訊息" \
    -- "$BIN" --system 你是傲嬌貓 你好 --endpoint "$FX/fake/chat/completions"
# 沒給 --system → body 不得出現 system 訊息（用 expect_out 反向：抓不到才算對，故改直接斷言）
if LLM_DUMP_BODY=1 "$BIN" 你好 --endpoint "$FX/fake/chat/completions" 2>&1 >/dev/null \
    | grep -qF '"role":"system"'; then
    printf '  [FAIL] 未給 --system 卻插了 system 訊息\n' >&2; fail=$((fail+1))
else
    printf '  [PASS] 未給 --system → body 無 system 訊息\n'; pass=$((pass+1))
fi

# ── (1b) tools / modalities：--tool 吐 JSON 行、--media-out 落檔 ──
echo "-- tools / modalities --"
printf '{"name":"get_weather","description":"查天氣","parameters":{"type":"object","properties":{"city":{"type":"string"}}}}' > "$TMP/tool.json"
expect_out '"tool":"get_weather"' "--tool → tool_calls 一行 JSON 吐 stdout" \
    -- "$BIN" 東京天氣 --tool "$TMP/tool.json" --endpoint "$FX/fake_tool/chat/completions"
expect_out '"city":"東京"' "--tool → arguments 原樣內嵌" \
    -- "$BIN" 東京天氣 --tool "$TMP/tool.json" --endpoint "$FX/fake_tool/chat/completions"
printf '{"voice":"alloy"}' > "$TMP/audio.json"
expect_out "llm-media-1.wav" "--modality＋--media-out → 媒體落檔、路徑吐 stdout" \
    -- "$BIN" 說你好 --modality "audio=$TMP/audio.json" --media-out "$TMP" --endpoint "$FX/fake_media/chat/completions"
if [ "$(cat "$TMP/llm-media-1.wav" 2>/dev/null)" = "hello-wav-bytes" ]; then
    printf '  [PASS] 落檔內容正確（base64 已解碼）\n'; pass=$((pass+1))
else
    printf '  [FAIL] 落檔內容不對：<%s>\n' "$(cat "$TMP/llm-media-1.wav" 2>/dev/null)" >&2; fail=$((fail+1))
fi
expect_exit 0 "媒體但沒給 --media-out → 明說丟棄、不炸" \
    -- "$BIN" 說你好 --endpoint "$FX/fake_media/chat/completions"

# ── (2) 設定來源：config 檔 + env 指路 + 旗標覆寫 ──
echo "-- 設定來源 --"
printf '{"endpoint":"%s","temperature":0.7}' "$FX/fake/chat/completions" > "$TMP/conf.json"
expect_out "才不是為你回答的" "--config 提供 endpoint（無 --endpoint 旗標）" \
    -- "$BIN" 你好 --config "$TMP/conf.json"
expect_out "才不是為你回答的" "env LLM_CLI_CONFIG 指路" \
    -- env LLM_CLI_CONFIG="$TMP/conf.json" "$BIN" 你好
expect_out '"name":"星野"' "命令列旗標覆寫 config 的 endpoint" \
    -- "$BIN" 你好 --config "$TMP/conf.json" --endpoint "$FX/fake_json/chat/completions"

# ── (3) 退出碼三段分流 ──
echo "-- 退出碼分流 --"
expect_exit 0 "成功" -- "$BIN" 你好 --endpoint "$FX/fake/chat/completions"
expect_exit 0 "--help" -- "$BIN" --help
expect_exit 1 "未知旗標" -- "$BIN" 你好 --bogus
expect_exit 1 "旗標缺值（--image）" -- "$BIN" 你好 --image
expect_exit 1 "缺 prompt（stdin 空）" -- sh -c "'$BIN' </dev/null"
expect_exit 1 "「-」但 stdin 空（拼完仍空）" -- sh -c "'$BIN' - </dev/null"
expect_exit 1 "--config 讀不到" -- "$BIN" 你好 --config /no/such.json
expect_exit 1 "--schema 檔讀不到" -- "$BIN" 你好 --schema /no/such.json
expect_exit 1 "--system 缺值" -- "$BIN" 你好 --system
printf '{bad json' > "$TMP/bad.json"
expect_exit 1 "config JSON 壞" -- "$BIN" 你好 --config "$TMP/bad.json"
expect_exit 1 "--tool 檔讀不到" -- "$BIN" 你好 --tool /no/such.json
expect_exit 1 "--tool JSON 壞" -- "$BIN" 你好 --tool "$TMP/bad.json"
expect_exit 1 "--modality 設定檔讀不到" -- "$BIN" 你好 --modality audio=/no/such.json
expect_exit 1 "--media-out 不是目錄" -- "$BIN" 你好 --media-out /no/such/dir
expect_exit 1 "數值旗標型別錯（--temperature abc）" \
    -- "$BIN" 你好 --temperature abc --endpoint "$FX/fake/chat/completions"
expect_exit 2 "請求失敗（連不上真 endpoint）" \
    -- "$BIN" 你好 --endpoint http://127.0.0.1:59999/v1/chat/completions --timeout-ms 800

echo "== 結果：PASS=$pass FAIL=$fail =="
[ "$fail" = 0 ]
