"""harness.py — 冒煙測試的管線：斷言、跑 CLI、記分。案例本身在 smoke.py。

跑 CLI 是「在同個行程內呼叫 cli_main」而非開 subprocess：快，且能自由替換
stdin（含模擬互動終端）。stdin 一律由本檔接管，不會去讀真正的終端而卡住。
"""
import io
import os
import sys
from contextlib import redirect_stderr, redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))   # handy 根
sys.path.insert(0, ROOT)

from util.llm import cli_main    # noqa: E402

FIX = os.path.join(HERE, "fixtures")
TEXT = "file://" + os.path.join(FIX, "text.json").replace("\\", "/")
TOOL = "file://" + os.path.join(FIX, "tool.json").replace("\\", "/")

RESULT = {"pass": 0, "fail": 0}


def check(desc, got, want):
    """單一斷言。"""
    if got == want:
        print("  PASS  " + desc)
        RESULT["pass"] += 1
        return
    print("  FAIL  " + desc)
    print("        got =%r" % (got,))
    print("        want=%r" % (want,))
    RESULT["fail"] += 1


class _Tty(io.StringIO):
    """假的互動終端 stdin：isatty() 回 True，CLI 就不會去讀它。"""

    def isatty(self):
        return True


def run_cli(args, stdin="", tty=False):
    """跑一次 cli_main，回 (退出碼, stdout＋stderr)。

    stdin＝要餵進去的文字（預設空，等同「導管接上但沒東西」）；
    tty=True 則模擬互動終端——CLI 應該完全不讀它。
    """
    out = io.StringIO()
    err = io.StringIO()
    saved = sys.stdin
    sys.stdin = _Tty("") if tty else io.StringIO(stdin)
    try:
        with redirect_stdout(out), redirect_stderr(err):
            code = cli_main(["llm"] + args)
    finally:
        sys.stdin = saved
    return code, out.getvalue() + err.getvalue()


def cli_case(desc, args, want_code, want_in="", stdin="", tty=False):
    """跑一個 CLI 案例，比對退出碼與輸出關鍵字。"""
    code, text = run_cli(args, stdin, tty)
    if code == want_code and (not want_in or want_in in text):
        print("  PASS  " + desc)
        RESULT["pass"] += 1
        return
    print("  FAIL  %s（退出碼 want=%d got=%d）" % (desc, want_code, code))
    print("        " + text.strip().replace("\n", " | ")[:120])
    RESULT["fail"] += 1


def report():
    """印總結，回退出碼。"""
    total = RESULT["pass"] + RESULT["fail"]
    print("\n結果：%d/%d 通過" % (RESULT["pass"], total))
    return 1 if RESULT["fail"] else 0
