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

# llme.json 的欄位 → util.llm 的旗標。沒填的欄位就不加。
FLAGS = [("endpoint", "--endpoint"), ("model", "--model"),
         ("timeout_ms", "--timeout-ms"), ("api_key", "--api-key")]


def main(argv):
    name = argv[0]        # 第一個參數＝endpoint 名
    rest = argv[1:]       # 其餘的原樣轉發

    settings = config.read(os.path.join(HERE, "llme.json"))
    chosen = settings[name]

    flags = []
    for field, flag in FLAGS:
        if chosen.get(field):
            flags.append(flag)
            flags.append(str(chosen[field]))

    # cli_main 跟一般 CLI 一樣會跳過第 0 個參數（程式名），所以前面墊一個。
    return cli_main(["llme"] + flags + rest)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
