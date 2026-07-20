#!/usr/bin/env python3
"""cli_smoke.py — cllm core-py CLI 的離線黑箱煙霧測試（仿 core/test/cli_smoke.sh）。

精神同 shell 版：CLI 是個可跑的程式，最誠實的測法就是「真的用 subprocess 跑它、驗 stdout／
退出碼」。全程走 file:// 假回應——直接複用 core/test/fixtures，不連網、不吃真後端。跨平台
（Windows + POSIX）：file:// URL 由 fixtures 絕對路徑（正斜線）拼成，內核的 _file_url_to_path
兩平台都解得動。

用：`python test/cli_smoke.py`（無需先安裝；用 -m cllm 跑，靠 PYTHONPATH 找到 package）。
全過回 0，任一敗回 1。本檔只做接線：共用底（斷言／計數／跑 CLI）在 smoke_harness.py，
測試分組在 smoke_cases_output.py（輸出／body）與 smoke_cases_exit.py（設定來源／退出碼）。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # 確保 sibling 模組可 import

import smoke_cases_exit
import smoke_cases_output
from smoke_harness import FX_ROOT, cleanup_temps, report


def main():
    if not os.path.isdir(FX_ROOT):
        print("找不到 fixtures：%s" % FX_ROOT, file=sys.stderr)
        return 1
    print("== cllm core-py CLI 離線煙霧測試 ==")
    print("fixtures: %s" % FX_ROOT)

    smoke_cases_output.run()
    smoke_cases_exit.run()

    cleanup_temps()
    return report()


if __name__ == "__main__":
    sys.exit(main())
