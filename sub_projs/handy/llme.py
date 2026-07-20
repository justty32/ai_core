#!/usr/bin/env python3
"""llme.py — pllm 之上的多 endpoint dispatcher（單檔）。

    llme <endpoint> [pllm 的其餘參數...]

把「換 endpoint 名」變成「換模型」：從 `llme.json` 挑一組 endpoint 設定（endpoint/model/
timeout_ms/api_key?），翻成 pllm 的連線旗標，其餘參數（prompt、--stream、--system、--schema、
--tool…）原樣透傳，再**重用 pllm 自己的 CLI 解析**跑一次呼叫——不 subprocess、不重造 flag 解析。

  llme deepseek --stream 你好      # → pllm --endpoint <ds> --model deepseek-chat … --api-key $DEEPSEEK_API_KEY --stream 你好
  llme deepseek 一句話介紹你自己    # deepseek＝雲端 DeepSeek（key 由 llme.json 的 $env 帶）
  llme local 你好                  # local＝本機 LM Studio（無 key；目前多離線）

設定檔 `llme.json`（多個模型都定義在那）由共用 lib `util.config` 讀，支援指令式節點：
  {"$env":"VAR"[,"default":…]}  取環境變數    {"$ref":"a.b.c"}  引用本檔其他值（共用區塊）
`_` 開頭的鍵非 endpoint（當共用資料，如 _lmstudio）。

## api_key 來源（cascade，前者優先）
  ① 使用者顯式 --api-key（在透傳參數裡）→ 尊重、不注入任何東西
  ② llme.json 該 endpoint 的 api_key（通常寫成 {"$env":"…"}）
  ③ env auto-inject：LLME_KEY_<EP> ＞ <EP>_API_KEY（<EP>＝endpoint 名大寫、非英數轉 _）
本地免 key 的 endpoint（②③都沒有）不受影響。

## 環境變數
  LLME_CONFIG  覆寫設定檔路徑（預設＝本檔同層 llme.json）
  LLME_DRY=1   不真打，改印「會傳給 pllm 的 argv」到 stderr 後 exit 0（冒煙自測用）
  LLME_KEY_<EP> / <EP>_API_KEY   auto-inject 的 api key 來源（見上③）

## 冒煙自測（不需真後端）
  LLME_DRY=1 ./llme.py local --stream 你好                       # local 無 key，不注入
  DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek hi          # $env 帶入 → …--api-key FAKE hi
  DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek --api-key mine hi  # 使用者自帶 → 不注入
  ./llme.py nonexist                                             # 找不到 → 列可用 endpoint，exit 2
  ./llme.py --help                                              # 用法＋可用 endpoint
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)     # 讓共用 lib util（handy/util/，namespace 套件）可 import
from util import config       # noqa: E402

CONFIG_PATH = os.environ.get("LLME_CONFIG") or os.path.join(HERE, "llme.json")

# config 欄位 → pllm 連線旗標（api_key 另走 cascade，不在此表）。缺的欄位跳過（用後端預設）。
FIELD_FLAGS = [("endpoint", "--endpoint"), ("model", "--model"), ("timeout_ms", "--timeout-ms")]


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


def _endpoints(cfg):
    """可用 endpoint 名＝非 `_` 開頭的鍵。"""
    return sorted(k for k in cfg if not k.startswith("_"))


def _usage(cfg, to=sys.stderr):
    to.write(
        "用法：llme <endpoint> [pllm 的其餘參數...]\n"
        "  例：llme deepseek --stream 你好\n"
        "  <endpoint> 取自 %s；其餘參數原樣透傳給 pllm（連線旗標已自動帶上）。\n"
        "  可用 endpoint：%s\n" % (CONFIG_PATH, " ".join(_endpoints(cfg))))


def main(argv):
    """argv＝sys.argv[1:]；回退出碼。"""
    try:
        cfg = config.read(CONFIG_PATH)  # 讀 + 解析 $env/$ref
    except (OSError, ValueError) as e:
        sys.stderr.write("llme：讀設定失敗（%s）：%s\n" % (CONFIG_PATH, e))
        return 2

    if not argv or argv[0] in ("--help", "-h"):
        _usage(cfg)
        return 0 if argv else 2

    ep, rest = argv[0], argv[1:]
    if ep.startswith("_") or not isinstance(cfg.get(ep), dict):
        sys.stderr.write("llme：找不到 endpoint「%s」\n" % ep)
        _usage(cfg)
        return 2
    ecfg = cfg[ep]

    # 組給 pllm 的 argv：假程式名 + 連線旗標 + [api_key] + 透傳。
    # 透傳放最後：pllm 命令列後者覆寫前者，使用者顯式 --model/--api-key 等仍最終生效。
    forwarded = ["llme"]
    for field, flag in FIELD_FLAGS:
        v = ecfg.get(field)
        if v is not None and v != "":
            forwarded += [flag, str(v)]
    if "--api-key" not in rest:                          # ① 使用者沒自帶才注入
        key = ecfg.get("api_key") or _auto_api_key(ep)   # ② config($env) ＞ ③ env auto-inject
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
