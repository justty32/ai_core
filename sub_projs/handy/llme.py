#!/usr/bin/env python3
"""llme <endpoint> [pllm 參數...]：從 llme.json 挑模型翻成 pllm 旗標透傳。用法見 README.md。"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)  # util 共用 lib
from util import config
CFG = os.environ.get("LLME_CONFIG") or os.path.join(HERE, "llme.json")
FLAGS = {"endpoint": "--endpoint", "model": "--model", "timeout_ms": "--timeout-ms"}
def auto_key(ep):  # env auto-inject：LLME_KEY_<EP> ＞ <EP>_API_KEY
    s = "".join(c if c.isalnum() else "_" for c in ep.upper())
    return os.environ.get("LLME_KEY_" + s) or os.environ.get(s + "_API_KEY")
def main(argv):
    try: cfg = config.read(CFG)
    except (OSError, ValueError) as e: print("llme：讀設定失敗：%s" % e, file=sys.stderr); return 2
    ep = argv[0] if argv else None
    if ep in (None, "--help", "-h") or not isinstance(cfg.get(ep), dict):
        print("用法：llme <endpoint> [pllm 參數...]\n可用：" + " ".join(sorted(k for k in cfg if not k.startswith("_"))), file=sys.stderr)
        return 0 if ep in ("--help", "-h") else 2
    ec, rest, fwd = cfg[ep], argv[1:], ["llme"]                       # ec＝該 endpoint 設定
    for f, flag in FLAGS.items():
        if ec.get(f) not in (None, ""): fwd += [flag, str(ec[f])]
    if "--api-key" not in rest and (k := ec.get("api_key") or auto_key(ep)): fwd += ["--api-key", k]
    fwd += rest                                                        # 透傳放最後＝使用者顯式旗標最終生效
    if os.environ.get("LLME_DRY"): print(" ".join(fwd[1:]), file=sys.stderr); return 0
    sys.path.insert(0, os.path.join(HERE, "pllm")); from pllm.cli import main as pm
    return pm(fwd)
if __name__ == "__main__":
    try: sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt: sys.exit(130)
