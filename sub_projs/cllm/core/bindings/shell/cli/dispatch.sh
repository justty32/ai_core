# dispatch.sh — 定位並 exec 真正的 `llm` 二進位（shell binding 的「內核」邊界）。
#
# 對齊位置：core-py 把「打 HTTP」拆到 http.py／「組請求＋解回應」拆到 request/response/stream。
# 在 shell，這些**全都在 `llm` 二進位裡**——本層的 binding 就是「shell-out 呼叫 `llm`」。所以
# 本檔就是那條 binding 邊界：把 argv 原封不動交給 `llm`，它才是真正做事的內核。
#
# 誠實交代（退化情形）：因為 binding＝直接 exec `llm`，這支 wrapper 對「一次發問」本質上是
# 透傳（passthrough）。本檔加的唯一價值＝二進位探測＋清楚的缺失診斷（比裸 `command not found`
# 友善），並用 exec 讓 stdin/stdout/退出碼/SIGINT 全部乾淨穿透、wrapper 不插手不污染。

[ -n "${_CLLM_DISPATCH_SOURCED:-}" ] && return 0
_CLLM_DISPATCH_SOURCED=1

# resolve_llm_bin — 回傳要 exec 的 `llm` 路徑到 stdout；找不到回非 0、不輸出。
# 解析序：$LLM_BIN（wrapper 覆寫）＞ PATH 上的 `llm`。刻意不找 `cllm`（那是 Python 平行實作，
# 非本 binding 目標）；要用它請自行 export LLM_BIN=cllm。
resolve_llm_bin() {
  local override="${!LLM_BIN_ENV_VAR:-}"
  if [ -n "$override" ]; then
    if command -v -- "$override" >/dev/null 2>&1; then command -v -- "$override"; return 0; fi
    _err "$LLM_BIN_ENV_VAR 指到「$override」但無法執行"; return 1
  fi
  command -v -- llm 2>/dev/null && return 0
  return 1
}

# run_llm — 解析二進位後 exec 它（本行程被取代＝完美透傳）；解析失敗回 EXIT_NO_LLM。
run_llm() {
  local bin
  if ! bin="$(resolve_llm_bin)"; then
    _err '找不到可執行的 `llm` 二進位。請先 `source ~/dev/cllm/env.sh`（把 $DEV/bin 放進 PATH），'
    _err "或 export $LLM_BIN_ENV_VAR=/絕對/路徑/llm 指定。這層只是薄殼，真正做事的是 llm。"
    return "$EXIT_NO_LLM"
  fi
  exec "$bin" "$@"   # exec：退出碼／SIGINT(130)／串流全由 `llm` 自負，wrapper 不再介入
}
