#!/usr/bin/env python3
"""llme.py — pllm 之上的多 endpoint dispatcher（單檔）。

    llme <endpoint> [pllm 的其餘參數...]

把「換 endpoint 名」變成「換模型」：挑一組內嵌 endpoint 設定（endpoint/model/timeout_ms），
翻成 pllm 的連線旗標，其餘參數（prompt、--stream、--system、--schema、--tool…）原樣透傳，
再**重用 pllm 自己的 CLI 解析**跑一次呼叫——不 subprocess、不重造任何 flag 解析。

  llme deepseek --stream 你好      # → pllm --endpoint <ds> --model deepseek-chat … --api-key $DEEPSEEK_API_KEY --stream 你好
  llme deepseek 一句話介紹你自己    # deepseek＝雲端 DeepSeek（key auto-inject）
  llme local 你好                  # local＝本機 LM Studio（無 key；目前多離線）

這是舊 Fennel 版 llme（folder-as-callable：_exec.fnl + configs/）用 pllm 重構的單檔版：
endpoint 設定從「configs/<ep>.json 檔」改為**內嵌 CONFIGS 表**（真單檔，加 endpoint＝改 dict）；
轉發目標從 shell-out 到 C++ `llm` CLI 改為 **import pllm、呼叫 pllm.cli.main(argv)**。

## auto-inject api key
呼叫 <endpoint> 時，使用者若**沒**自帶 --api-key，依序找環境變數、找到就自動補：
  ① LLME_KEY_<EP>（llme 專屬覆寫）  ② <EP>_API_KEY（通用 provider 慣例，如 DEEPSEEK_API_KEY）
<EP>＝endpoint 名大寫、非英數轉 `_`。對應 env 沒設（如 local）→ 不注入；使用者顯式 --api-key
→ 尊重、不覆寫（keyless 設定的 secret 不落版控）。

## 環境變數
  LLME_DRY=1   不真打，改印「會傳給 pllm 的 argv」到 stderr 後 exit 0（冒煙自測用，仿舊 LLME_LLM=echo）
  LLME_KEY_<EP> / <EP>_API_KEY   auto-inject 的 api key 來源（見上）

## 冒煙自測（不需真後端）
  LLME_DRY=1 ./llme.py local --stream 你好                       # local 無 key，不注入
  DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek hi          # auto-inject → …--api-key FAKE hi
  DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek --api-key mine hi  # 使用者自帶 → 不注入
  ./llme.py nonexist                                             # 找不到 → 列可用 endpoint，exit 2
  ./llme.py --help                                              # 用法＋可用 endpoint
"""
import os
import sys

# ── 內嵌 endpoint 表（加 endpoint＝在此加一列；欄位對齊 pllm 連線旗標）──
CONFIGS = {
    # handy 預設後端：雲端 DeepSeek（keyless，key 靠 auto-inject DEEPSEEK_API_KEY）
    "deepseek": {"endpoint": "https://api.deepseek.com/v1/chat/completions",
                 "model": "deepseek-chat", "timeout_ms": 120000},
    # 本機 LM Studio（無 key；目前多離線省電）
    "local": {"endpoint": "http://localhost:1234/v1/chat/completions",
              "model": "google/gemma-4-e4b", "timeout_ms": 120000},
    "qwen": {"endpoint": "http://localhost:1234/v1/chat/completions",
             "model": "qwen/qwen3.5-9b", "timeout_ms": 120000},
}

HERE = os.path.dirname(os.path.abspath(__file__))


def _import_pllm_main():
    """import 同層 pllm/ 裡的 pllm 套件，回其 cli.main（免 pip install 也能跑）。"""
    sys.path.insert(0, os.path.join(HERE, "pllm"))  # handy/pllm/ 下有 pllm 套件
    from pllm.cli import main  # noqa: E402
    return main


def _env_stem(ep):
    """endpoint 名 → env 慣例識別子：大寫、非英數轉 _（deep-seek → DEEP_SEEK）。"""
    return "".join(c if c.isalnum() else "_" for c in ep.upper())


def _auto_api_key(ep):
    """LLME_KEY_<EP> ＞ <EP>_API_KEY（皆需 nonempty）；都沒有回 None。"""
    stem = _env_stem(ep)
    for key in ("LLME_KEY_" + stem, stem + "_API_KEY"):
        v = os.environ.get(key)
        if v:
            return v
    return None


def _usage(to=sys.stderr):
    to.write(
        "用法：llme <endpoint> [pllm 的其餘參數...]\n"
        "  例：llme deepseek --stream 你好\n"
        "  <endpoint> 挑一組內嵌設定，其餘參數原樣透傳給 pllm（--endpoint/--model/--timeout-ms 已自動帶上）。\n"
        "  可用 endpoint：" + " ".join(sorted(CONFIGS)) + "\n")


def main(argv):
    """argv＝sys.argv[1:]；回退出碼。"""
    if not argv or argv[0] in ("--help", "-h"):
        _usage()
        return 0 if argv else 2

    ep, rest = argv[0], argv[1:]
    cfg = CONFIGS.get(ep)
    if cfg is None:
        sys.stderr.write("llme：找不到 endpoint「%s」\n" % ep)
        _usage()
        return 2

    # 組給 pllm 的 argv：假程式名 + 連線旗標（config 來） + [auto --api-key] + 透傳。
    # 透傳放最後：pllm 命令列後者覆寫前者，使用者顯式 --model/--api-key 等仍最終生效。
    forwarded = ["llme",
                 "--endpoint", cfg["endpoint"],
                 "--model", cfg["model"],
                 "--timeout-ms", str(cfg["timeout_ms"])]
    if "--api-key" not in rest:  # 使用者沒自帶才 auto-inject
        key = _auto_api_key(ep)
        if key:
            forwarded += ["--api-key", key]
    forwarded += rest

    if os.environ.get("LLME_DRY"):  # 冒煙自測：只印會傳出的 argv，不真打
        sys.stderr.write(" ".join(forwarded[1:]) + "\n")
        return 0

    return _import_pllm_main()(forwarded)


def _entry():
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.stderr.write("\n已取消\n")
        sys.exit(130)


if __name__ == "__main__":
    _entry()
