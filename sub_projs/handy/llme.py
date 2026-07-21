#!/usr/bin/env python3
"""llme <endpoint> [util.llm 參數...]：從 llme.json 挑模型翻成旗標透傳。

換 endpoint 名＝換模型。完整用法／設定格式見 README.md。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # 讓共用 lib util（handy/util/）可 import

from util import config          # noqa: E402
from util.llm import cli_main    # noqa: E402

CFG = os.environ.get("LLME_CONFIG") or os.path.join(HERE, "llme.json")

# config 欄位 → 連線旗標。缺的欄位跳過。
FLAGS = [("endpoint", "--endpoint"), ("model", "--model"),
         ("timeout_ms", "--timeout-ms"), ("api_key", "--api-key")]


def main(argv):
    cfg = config.read(CFG)
    ep = argv[0] if argv else None
    if ep in (None, "--help", "-h") or not isinstance(cfg.get(ep), dict):
        eps = " ".join(sorted(k for k in cfg if not k.startswith("_")))
        sys.stderr.write("用法：llme <endpoint> [util.llm 參數...]\n"
                         "可用 endpoint：" + eps + "\n")
        return 0 if ep in ("--help", "-h") else 2

    ecfg = cfg[ep]
    fwd = ["llme"]  # 假程式名（util.llm 的 argv 解析跳過 argv[0]）
    for field, flag in FLAGS:
        if ecfg.get(field) not in (None, ""):
            fwd += [flag, str(ecfg[field])]
    return cli_main(fwd + argv[1:])  # 透傳在後＝使用者的同名旗標最終生效


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
