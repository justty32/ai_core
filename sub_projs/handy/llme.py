#!/usr/bin/env python3
"""llme <endpoint> [util.llm 參數...]：從 llme.json 挑模型翻成旗標透傳。

換 endpoint 名＝換模型。llme.json 裡的欄位名就是旗標名（底線換連字號），
所以想固定 temperature 之類的，直接在該 endpoint 加一欄即可。
完整用法／設定格式見 README.md。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # 讓共用 lib util（handy/util/）可 import

from util import config          # noqa: E402
from util.llm import cli_main    # noqa: E402


def main(argv):
    name = argv[0]        # 第一個參數＝endpoint 名
    rest = argv[1:]       # 其餘的原樣轉發

    settings = config.read(os.path.join(HERE, "llme.json"))
    chosen = settings[name]

    flags = []
    for field, value in chosen.items():
        if value:                                  # 沒填的欄位就不加
            flags.append("--" + field.replace("_", "-"))
            flags.append(str(value))

    # cli_main 跟一般 CLI 一樣會跳過第 0 個參數（程式名），所以前面墊一個。
    return cli_main(["llme"] + flags + rest)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
