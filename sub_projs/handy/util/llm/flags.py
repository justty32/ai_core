"""flags.py — 反射旗標表與 --help 用法文字（沿用已封存 pllm 的同名檔）。

反射旗標＝連線／取樣選項，逐項對應一個 ask() 的 kwarg；固定旗標（--stream／
--image／…）的解析在 argv.py。print_usage 印的清單即由本表生成。
"""
import sys

from .errors import CONFIG_ENV_VAR

# 反射旗標表：--flag → (欄位名, 型別)。欄位名即 ask() 的 kwarg 名。
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

FLAG_TO_FIELD = {}
for _flag, _field, _typ in REFLECT_FLAGS:
    FLAG_TO_FIELD[_flag] = (_field, _typ)

# --help 用的欄位標註（範圍為 OpenAI 慣例值）。
FIELD_ANNOT = {
    "temperature": "，範圍 0.0–2.0，例 0.7（越大越發散）",
    "top_p": "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）",
    "presence_penalty": "，範圍 -2.0–2.0，例 0.0",
    "frequency_penalty": "，範圍 -2.0–2.0，例 0.0",
    "max_tokens": "，≥1，例 512（⚠ reasoning 模型建議不設）",
    "seed": "，例 42（固定可重現）",
    "timeout_ms": "，≥0（0＝不設限），例 120000",
    "endpoint": "，收完整 URL，例 http://host/v1/chat/completions",
    "model": "，例 local-model（自動加 openai/ 前綴）",
    "api_key": "，雲端 API 必給",
}
TYPE_HINT = {str: "<字串>", int: "<整數>", float: "<小數>"}

_HEAD = (
    "用法：llm [旗標...] [prompt...]        # 旗標與 prompt 可交錯\n"
    "  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫\n"
    "  「-」而兩者都有＝prompt＋空行＋stdin；只給其一＝用那一個。\n"
    "  範例：llm 用一句話介紹你自己\n"
    "        cat report.txt | llm 總結這份       # prompt＋stdin 合體\n"
    "        git diff | llm 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點\n"
    "        llm --stream -- --開頭的-prompt      # -- 之後一律當 prompt\n"
    "\n固定旗標：\n"
    "  --system <文字>        system role 指示（字面文字；長文用 $(cat p.txt)）\n"
    "  --stream               串流逐段吐 stdout（布林，無值）\n"
    "  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:／http(s)://\n"
    "                         URL 直接當 url 送；.json 檔＝預先算好的描述子，\n"
    "                         直通不重算 base64；其餘＝圖檔路徑，讀檔＋base64\n"
    "  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出\n"
    "                         （⚠ 收字面非路徑；長內容用 $(cat s.json)）\n"
    "  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）\n"
    "  --tool <JSON>          字面工具定義 JSON（可重複），需 name 與\n"
    "                         parameters；tool_calls 一行一則 JSON 吐 stdout\n"
    "  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或\n"
    "                         audio={\"voice\":\"alloy\"}（=後收字面 JSON）\n"
    "  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐\n"
    "                         stdout）；沒給＝丟棄\n"
    "  --                     分隔符：其後所有參數一律當 prompt\n"
    "  --help, -h             顯示本說明\n"
    "\n連線／取樣旗標（未給即不送、交後端默認）：\n")

_TAIL = (
    "\n（數值範圍為 OpenAI 慣例，實際上下限依後端而定。）\n"
    "\n設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。\n"
    "config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。\n"
    "  （env 只用來指定 config 檔路徑，不存任何設定值。）\n"
    "後端＝litellm；LLM_RAW_MODEL=1 可關掉 model 的 openai/ 前綴。\n"
    "離線自測：--endpoint file://<絕對路徑> 指向一份假回應 JSON。\n")


def print_usage(out=None):
    """印 --help（輸出到 stderr）。"""
    if out is None:
        out = sys.stderr
    out.write(_HEAD)
    for flag, field, typ in REFLECT_FLAGS:
        out.write("  %-22s %s%s\n"
                  % (flag, TYPE_HINT[typ], FIELD_ANNOT.get(field, "")))
    out.write(_TAIL % CONFIG_ENV_VAR)
