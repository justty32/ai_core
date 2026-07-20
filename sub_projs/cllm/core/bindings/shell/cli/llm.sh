#!/usr/bin/env bash
# llm.sh — Shell CLI 薄外殼的進入點（對齊 core/src/cli.cpp 的 main 接線）。
#
# 進入點：bash cli/llm.sh [任何 llm 旗標...] [prompt...]
#
# 職責切分（比照 core-py，各檔 <150 行、單一職責；括號＝對齊的 C++ 檔）：
#   internal.sh（cli_internal.hpp）＝退出碼／env 鍵／小工具（葉）
#   flags.sh   （cli_flags.cpp）  ＝wrapper 自我描述 print_usage
#   config.sh  （cli_config.cpp） ＝三層 config 來源「探測與報告」（只探測、不解析）
#   dispatch.sh（http.cpp 邊界）  ＝定位並 exec `llm`——shell binding 的內核邊界（binding＝shell-out）
#   llm.sh     （cli.cpp）        ＝本檔：攔 wrapper 專屬旗標，其餘全透傳
#
# ⚠ 刻意「沒有」的對應（誠實交代退化情形）：core-py 的 argv.py／reqinput.py／media.py／output.py
# 在此**無對應檔**——因為 argv 掃描、請求組裝、--image 三分流、四路輸出 Sink（文字/tool/媒體/錯誤）
# 全由 `llm` 二進位自己做。在 shell 重造它們＝把 `llm` 已有的邏輯再包一遍，純屬過度工程。所以本殼
# 只保留「探測＋透傳＋自我描述」三件事，其餘老實委派給 `llm`。

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=internal.sh
. "$HERE/internal.sh"
. "$HERE/flags.sh"
. "$HERE/config.sh"
. "$HERE/dispatch.sh"

main() {
  # ── wrapper 專屬旗標：僅當 argv[1] 時攔截（避免吃到本該當 prompt 的字；要送字面值用 `-- ...`）──
  case "${1:-}" in
    --meta)
      wrapper_usage
      printf '\n── 實際解析 ──\n' >&2
      local bin
      if bin="$(resolve_llm_bin)"; then
        printf 'llm 二進位＝%s\n' "$bin"
      else
        printf 'llm 二進位＝（找不到！先 source ~/dev/cllm/env.sh 或 export %s）\n' "$LLM_BIN_ENV_VAR"
      fi
      shift
      report_config_source "$@"
      return "$EXIT_OK"
      ;;
    --wrapper-help)
      wrapper_usage
      return "$EXIT_OK"
      ;;
  esac

  # ── 其餘一律原封透傳給 `llm`（exec；退出碼由 llm 自負）──
  run_llm "$@"
}

main "$@"
