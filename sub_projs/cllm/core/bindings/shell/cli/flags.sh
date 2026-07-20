# flags.sh — wrapper 自身的說明文字（對齊 core/src/cli_flags.cpp 的 print_usage）。
#
# ⚠ 誠實交代：canonical 旗標面（--stream/--schema/--tool/--image/--modality/--media-out ＋
# 連線／取樣旗標）**全由 `llm` 反射生成並解析**，不在本層。所以本檔的 print_usage 不重列那張
# 表（會過時、且不是這層的真相），而是：說明本 wrapper 是 `llm` 的薄透傳殼、列出 wrapper 專屬的
# 少數旗標、把權威旗標清單導到 `llm --help`。

[ -n "${_CLLM_FLAGS_SOURCED:-}" ] && return 0
_CLLM_FLAGS_SOURCED=1

# wrapper_usage — 印本殼的用法／自我描述到 stderr。被 --meta 與缺二進位診斷共用。
wrapper_usage() {
  cat >&2 <<'USAGE'
llm.sh — cllm 的 Shell CLI 薄外殼（binding＝直接 shell-out 呼叫 `llm`）

  用法：bash cli/llm.sh [任何 llm 旗標...] [prompt...]
        bash cli/llm.sh --meta          # 印本殼自我描述（llm 路徑＋config 來源），不呼叫 llm

  這一層做什麼：把 argv/stdin/stdout/退出碼**原封透傳**給 `llm` 二進位（用 exec）。真正組請求、
  打 HTTP、解串流、跑 tools、落媒體、定退出碼（0/1/2/130）的全是 `llm` 自己。

  這一層的（少量）附加價值：
    • 二進位探測＋清楚診斷——找不到 `llm` 時給人話（含 EXIT_NO_LLM=127），而非裸 command not found。
    • `--meta` 自我描述——報告解析到的 `llm` 路徑與這次會用的 config 檔（探測、不解析）。
    • 用 exec 完美透傳——不吞 stdin、不改 stdout、不動退出碼與 SIGINT。

  wrapper 專屬旗標（僅當作 argv[1] 時攔截；其餘一律透傳給 llm）：
    --meta            印自我描述後 exit 0，不呼叫 llm
    --wrapper-help    印本說明後 exit 0（與 --meta 同源；`--help`/`-h` 會透傳給 llm 看權威說明）

  權威旗標清單（--stream/--schema/--tool/--image/--modality/--media-out／連線取樣旗標…）：
    ⇒ 執行 `llm --help`（那張表由 llm 從 Client 欄位反射生成，是唯一真相源）。

  離線自測：bash cli/smoke.sh（走 $CLLM_FIXTURES 的 file:// 假回應）。
USAGE
}
