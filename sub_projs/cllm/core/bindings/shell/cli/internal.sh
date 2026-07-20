# internal.sh — CLI 共用的退出碼、環境變數鍵、小工具（對齊 core/src/cli_internal.hpp）。
#
# 葉模組：不 source 其他 cli/ 檔，只被 flags/config/dispatch/llm 共 source，把依賴收成無環 DAG。
# 用法：由進入點 `source "$HERE/internal.sh"`；含 include guard，重複 source 無害。
#
# ⚠ shell 特殊性（誠實交代）：本層的「binding」＝直接 shell-out 呼叫 `llm`（見 dispatch.sh），
# 所以退出碼 0/1/2/130 幾乎全由 `llm` 自己吐、原封透傳；本檔只多定一個 wrapper 專屬碼
# EXIT_NO_LLM（找不到 `llm` 二進位），那是這層唯一自己會產生的失敗。

[ -n "${_CLLM_INTERNAL_SOURCED:-}" ] && return 0
_CLLM_INTERNAL_SOURCED=1

# ── 退出碼（對齊 cli_internal.hpp；由 `llm` 透傳）──
readonly EXIT_OK=0        # 成功（含 --meta、llm 正常回答）
readonly EXIT_USAGE=1     # 用法錯（llm 判定：未知旗標／缺 prompt／檔案讀不到／JSON 壞…）
readonly EXIT_REQUEST=2   # 請求失敗（llm 判定：連不上 endpoint／後端錯／媒體落檔失敗）
readonly EXIT_CANCEL=130  # SIGINT 取消（llm 收到 Ctrl-C；exec 後由 llm 自行處理）
# ── wrapper 專屬（這層唯一自產的失敗；沿用 POSIX「command not found」慣例）──
readonly EXIT_NO_LLM=127  # 找不到可執行的 `llm` 二進位（環境未裝／LLM_BIN 指錯）

# ── 環境變數鍵 ──
readonly CONFIG_ENV_VAR="LLM_CLI_CONFIG"  # 對齊 cli_internal.hpp：只指定 config 檔路徑（llm 讀）
readonly LLM_BIN_ENV_VAR="LLM_BIN"        # wrapper 專屬：覆寫要 exec 的 `llm` 二進位路徑

# 印一行診斷到 stderr（統一前綴，便於 grep）。
_err() { printf 'llm.sh: %s\n' "$*" >&2; }
