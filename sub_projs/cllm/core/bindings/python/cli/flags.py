"""flags.py — 反射旗標表與 --help 用法文字（對齊 core/src/cli_flags.cpp）。

反射旗標＝連線／取樣選項，逐項對應 llm.ask 的 Client 類參數（＝ C ABI 的 llm_client_t 欄位）；
固定旗標（--stream/--image/…）的解析在 argv.py。print_usage 印的清單即由本表生成。
感覺基準＝core-py 的 cllm/flags.py。
"""
import sys

from internal import CONFIG_ENV_VAR

# 反射旗標表：--flag → (llm.ask 參數名, 型別)。順序／欄位對齊 C ABI 的 llm_client_t（cabi_client.h）。
REFLECT_FLAGS = [
    ("--endpoint", "endpoint", str),
    ("--api-key", "api_key", str),
    ("--timeout-ms", "timeout_ms", int),
    ("--model", "model", str),
    ("--temperature", "temperature", float),
    ("--top-p", "top_p", float),
    ("--presence-penalty", "presence_penalty", float),
    ("--frequency-penalty", "frequency_penalty", float),
    ("--max-tokens", "max_tokens", int),
    ("--seed", "seed", int),
]
FLAG_TO_FIELD = {flag: (field, typ) for flag, field, typ in REFLECT_FLAGS}

# --help 用的欄位標註（對齊 cli_flags.cpp 的 field_annot；範圍為 OpenAI 慣例值）。
FIELD_ANNOT = {
    "temperature": "，範圍 0.0–2.0，例 0.7（越大越發散）",
    "top_p": "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）",
    "presence_penalty": "，範圍 -2.0–2.0，例 0.0",
    "frequency_penalty": "，範圍 -2.0–2.0，例 0.0",
    "max_tokens": "，≥1，例 512（⚠ reasoning 模型建議不設）",
    "seed": "，例 42（固定可重現）",
    "timeout_ms": "，≥0（0＝不設限），例 120000",
    "endpoint": "，例 http://localhost:1234/v1/chat/completions",
    "model": "，例 local-model（雲端填真實 model id）",
    "api_key": "，雲端 API 必給",
}
TYPE_HINT = {str: "<字串>", int: "<整數>", float: "<小數>"}


def print_usage(out=None):
    """印 --help（對齊 cli_flags.cpp 的 print_usage；輸出到 stderr）。"""
    out = out or sys.stderr
    out.write(
        "用法：llm [旗標...] [prompt...]        # 旗標與 prompt 可交錯\n"
        "  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有\n"
        "  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。\n"
        "  範例：llm 用一句話介紹你自己\n"
        "        cat report.txt | llm 總結這份       # prompt＋stdin 合體\n"
        "        git diff | llm 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點\n"
        "        llm --stream -- --開頭的-prompt      # -- 之後一律當 prompt（unix 分隔符）\n"
        "\n固定旗標：\n"
        "  --system <文字>        system role 指示（字面文字；長文用 --system \"$(cat persona.txt)\"）\n"
        "  --stream               串流逐段吐 stdout（布林，無值）\n"
        "  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:/http(s):// URL 直接當 url 送；\n"
        "                         .json 檔＝預先算好的 media 描述子（{\"url\":…} 或 {\"mime\":…,\"data\":…}）\n"
        "                         直通、不重算 base64；其餘＝二進位圖檔路徑，讀檔＋base64（mime 由副檔名推）\n"
        "  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出（⚠ 收字面非路徑；長內容用 $(cat s.json)）\n"
        "  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）\n"
        "  --tool <JSON>          字面工具定義 JSON（可重複）：{\"name\",\"description\",\"parameters\"}；\n"
        "                         ⚠ 收字面非路徑，長內容用 $(cat t.json)；tool_calls 一行一則 JSON 吐 stdout\n"
        "  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或 audio={\"voice\":\"alloy\"}（=後收字面 JSON）\n"
        "  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄\n"
        "  --                     分隔符：其後所有參數一律當 prompt\n"
        "  --help, -h             顯示本說明\n"
        "\n連線／取樣旗標（對應 Client 欄位，未給即不送、交後端默認）：\n")
    for flag, field, typ in REFLECT_FLAGS:
        out.write("  %-22s %s%s\n" % (flag, TYPE_HINT[typ], FIELD_ANNOT.get(field, "")))
    out.write(
        "\n（數值範圍為 OpenAI 慣例，實際上下限依後端而定。）\n"
        "\n設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。\n"
        "config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。\n"
        "  （env 只用來指定 config 檔路徑，不存任何設定值。）\n"
        "離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。\n" % CONFIG_ENV_VAR)
