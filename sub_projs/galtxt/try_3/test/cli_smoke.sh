#!/usr/bin/env bash
# cli_smoke.sh — galtxt try_3 `llm` CLI 的離線黑箱煙霧測試。
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
FX="file://$ROOT/test/fixtures"
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
printf '{"type":"object"}' > "$TMP/sc.json"
expect_out '"name":"星野"' "--schema 結構化輸出" \
    -- "$BIN" 給我角色 --schema "$TMP/sc.json" --endpoint "$FX/fake_json/chat/completions"

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
expect_exit 1 "--config 讀不到" -- "$BIN" 你好 --config /no/such.json
expect_exit 1 "--schema 檔讀不到" -- "$BIN" 你好 --schema /no/such.json
printf '{bad json' > "$TMP/bad.json"
expect_exit 1 "config JSON 壞" -- "$BIN" 你好 --config "$TMP/bad.json"
expect_exit 1 "數值旗標型別錯（--temperature abc）" \
    -- "$BIN" 你好 --temperature abc --endpoint "$FX/fake/chat/completions"
expect_exit 2 "請求失敗（連不上真 endpoint）" \
    -- "$BIN" 你好 --endpoint http://127.0.0.1:59999/v1/chat/completions --timeout-ms 800

echo "== 結果：PASS=$pass FAIL=$fail =="
[ "$fail" = 0 ]
