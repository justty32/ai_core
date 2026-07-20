# config.sh — 三層 config 來源「探測與報告」（對齊 core/src/cli_config.cpp 的路徑解析段）。
#
# ⚠ 只探測、不解析、不套用：真正把 config 灌進請求的是 `llm`（glaze 反射整份讀入）。本檔存在
# 純為 `--meta` 誠實報告「這次 `llm` 會用哪個 config 檔」，不重造 llm 已有的解析邏輯。
#
# 路徑解析序（對齊 cli_config.cpp，後者覆寫前者的是「值」，這裡只挑「路徑」）：
#   --config <檔>（命令列明指） ＞ 環境變數 LLM_CLI_CONFIG（明指） ＞ ~/.config/llm/config.json（探測）

[ -n "${_CLLM_CONFIG_SOURCED:-}" ] && return 0
_CLLM_CONFIG_SOURCED=1

# _scan_config_flag — 從整串 argv 撈 `--config <值>`（回最後一個；沒有回空）。
# 供 --meta 報告用；wrapper 不攔 --config，它照樣透傳給 `llm`。
_scan_config_flag() {
  local prev="" a val=""
  for a in "$@"; do
    [ "$prev" = "--config" ] && val="$a"
    prev="$a"
  done
  printf '%s' "$val"
}

# report_config_source — 印一行「config 來源＝<路徑>（<層級>，<存在與否>）」。收 wrapper 的 argv。
report_config_source() {
  local path level from_flag
  from_flag="$(_scan_config_flag "$@")"
  if [ -n "$from_flag" ]; then
    path="$from_flag"; level="--config 明指"
  elif [ -n "${!CONFIG_ENV_VAR:-}" ]; then
    path="${!CONFIG_ENV_VAR}"; level="環境變數 $CONFIG_ENV_VAR 明指"
  elif [ -n "${HOME:-}" ]; then
    path="$HOME/.config/llm/config.json"; level="預設探測路徑"
  else
    printf 'config 來源＝（無 HOME，無預設路徑）\n'; return 0
  fi
  if [ -f "$path" ]; then
    printf 'config 來源＝%s（%s，存在）\n' "$path" "$level"
  else
    printf 'config 來源＝%s（%s，不存在→llm 用內建預設）\n' "$path" "$level"
  fi
}
