#!/usr/bin/env python3
"""llme <endpoint> [util.llm 參數...]：從 llme.json 挑模型翻成旗標透傳。

換 endpoint 名＝換模型。完整用法／設定格式／api_key 來源見 README.md。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # 讓共用 lib util（handy/util/）可 import

from util import config, env       # noqa: E402
from util.llm import cli_main      # noqa: E402

CFG = os.environ.get("LLME_CONFIG") or os.path.join(HERE, "llme.json")

# config 欄位 → 連線旗標（api_key 另走 cascade）。缺的欄位跳過。
FLAGS = [("endpoint", "--endpoint"), ("model", "--model"),
         ("timeout_ms", "--timeout-ms")]


def main(argv):
    try:
        cfg = config.read(CFG)
    except (OSError, ValueError) as e:
        sys.stderr.write("llme：讀設定失敗：%s\n" % e)
        return 2

    ep = argv[0] if argv else None
    if ep in (None, "--help", "-h") or not isinstance(cfg.get(ep), dict):
        eps = " ".join(sorted(k for k in cfg if not k.startswith("_")))
        sys.stderr.write("用法：llme <endpoint> [util.llm 參數...]\n"
                         "可用 endpoint：" + eps + "\n")
        return 0 if ep in ("--help", "-h") else 2

    ecfg = cfg[ep]
    rest = argv[1:]
    fwd = ["llme"]  # 假程式名（util.llm 的 argv 解析跳過 argv[0]）
    for field, flag in FLAGS:
        if ecfg.get(field) not in (None, ""):
            fwd += [flag, str(ecfg[field])]

    # api_key cascade：使用者 --api-key ＞ config（$env）＞ env auto-inject。
    if "--api-key" not in rest:
        stem = env.stem(ep)   # 名字大寫成 ENV_IDENT
        key = ecfg.get("api_key") or env.first("LLME_KEY_" + stem,
                                               stem + "_API_KEY")
        if key:
            fwd += ["--api-key", key]
    fwd += rest  # 透傳放最後＝使用者顯式旗標最終生效

    if os.environ.get("LLME_DRY"):  # 冒煙自測：只印會傳出的 argv
        sys.stderr.write(" ".join(fwd[1:]) + "\n")
        return 0
    return cli_main(fwd)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.exit(130)
