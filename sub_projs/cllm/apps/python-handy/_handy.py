# _handy.py — python-handy 四工具（llme/zhtw/wf/mail）共用的跨平台進程膠水。
#
# 為什麼存在：原本各工具用 `subprocess.call(" ".join(parts), shell=True)` ＋ POSIX
# `shlex.quote` 組一行丟給 shell。這在 Windows 會壞——`shell=True` 走 cmd.exe，POSIX 的
# 單引號不是引號（同 cllm C++ 版的 popen 單引號坑，見 gotchas/windows.md）。這裡改成
# **list-form（argv 陣列、無 shell）**：轉義交給 OS，跨平台一致、也免了 shlex。
#
# 三個 Windows 專屬處理（POSIX 行為不變）：
#   ① 我方 sibling 是無副檔名的 Python script（llme/mail）——Windows 無 shebang exec，
#      故一律以 `[sys.executable, script, …]` 起（py=True）。
#   ② 外部命令若是 .cmd/.bat（如 npm 裝的 claude.cmd）——Windows CreateProcess 不直接跑
#      批次檔，需 `cmd /c`。用 shutil.which 找真路徑後判副檔名決定要不要包 cmd /c。
#   ③ 離線 dry-run 慣用 `LLME_LLM=echo` / `WF_CLAUDE=echo`——Windows 無 echo.exe（cmd 內建），
#      list-form 找不到 → 這裡把 `echo` 轉接到同層 _echo.py 蟬（輸出與 echo 一致），
#      讓 dry-run 在 Windows 與 POSIX 表現相同、README 範例免分platform。
import os
import sys
import shutil
import subprocess
from pathlib import Path

_DIR = Path(__file__).resolve().parent


def getenv_nonempty(k):
    v = os.environ.get(k)
    return v if v else None


def sibling(name):
    """同層 sibling 檔的絕對路徑（工具互呼、_echo.py 蟬都用）。"""
    return str(_DIR / name)


def _resolve_external(prog):
    """外部命令名 → 可交給 CreateProcess/execvp 的 argv 前綴（list）。"""
    p = str(prog)
    if os.name == "nt" and p == "echo":
        # Windows 無 echo.exe：轉接同層蟬，輸出對齊 POSIX `echo <args>`。
        return [sys.executable, sibling("_echo.py")]
    found = shutil.which(p) or p
    if os.name == "nt" and found.lower().endswith((".cmd", ".bat")):
        return ["cmd", "/c", found]  # 批次檔需經 cmd /c
    return [found]


def _argv(prog, args, py):
    if py or str(prog).endswith(".py"):
        return [sys.executable, str(prog)] + list(args)
    return _resolve_external(prog) + list(args)


def spawn(prog, args, *, py=False, input=None):
    """執行 prog+args（list-form、無 shell），透傳 exit code。
       py=True＝prog 是我方 Python script（走 sys.executable）；input＝餵給子程序 stdin 的 bytes。"""
    r = subprocess.run(_argv(prog, args, py), input=input)
    return r.returncode


def capture(prog, args, *, py=False, input=None):
    """同 spawn 但捕捉 stdout，回 decode 後的字串（utf-8, replace）。"""
    r = subprocess.run(_argv(prog, args, py), input=input, stdout=subprocess.PIPE)
    return (r.stdout or b"").decode("utf-8", "replace")
