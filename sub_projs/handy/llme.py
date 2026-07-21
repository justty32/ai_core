#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["litellm"]
# ///
"""llme <endpoint> [util.llm 參數...]：從 llme.json 挑模型翻成旗標透傳。

換 endpoint 名＝換模型。llme.json 裡的欄位名就是旗標名（底線換連字號），
所以想固定 temperature 之類的，直接在該 endpoint 加一欄即可。
命令列已自帶的旗標，設定檔的同名欄位就不送（使用者說的算）。
完整用法／設定格式見 README.md。
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # 讓共用 lib util（handy/util/）可 import

from util import config  # noqa: E402
from util.llm import cli_main  # noqa: E402
from util.llm.flags import FLAG_TO_FIELD  # noqa: E402

# 會吃下一個參數當值的固定旗標（鏡像 util/llm/argv.py 的同一份清單）。
VALUED = ("--image", "--media", "--schema", "--system", "--config",
          "--tool", "--modality", "--media-out")


def given_flags(rest):
    """掃出使用者自己在命令列給了哪些連線／取樣旗標（`--` 之後一律當 prompt）。"""
    seen = set()
    i = 0
    while i < len(rest):
        a = rest[i]
        if a == "--":
            break
        if a in FLAG_TO_FIELD:
            seen.add(a)
        if a in FLAG_TO_FIELD or a in VALUED:
            i += 1  # 跳過它的值，免得值長得像旗標時誤判
        i += 1
    return seen


def main(argv):
    name = argv[0]  # 第一個參數＝endpoint 名
    rest = argv[1:]  # 其餘的原樣轉發

    settings = config.read(os.path.join(HERE, "llme.json"))
    chosen = settings[name]

    given = given_flags(rest)
    flags = []
    for field, value in chosen.items():
        flag = "--" + field.replace("_", "-")
        if value is None:      # 沒填（null／$env 沒設）的欄位就不加
            continue
        if flag in given:      # 命令列已經自己給了＝不拿設定檔去蓋
            continue
        flags.extend([flag, str(value)])

    # cli_main 跟一般 CLI 一樣會跳過第 0 個參數（程式名），所以前面墊一個。
    return cli_main(["llme"] + flags + rest)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
