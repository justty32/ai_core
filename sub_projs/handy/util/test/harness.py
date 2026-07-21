"""harness.py — util 測試的管線：斷言、寫暫存 JSON、記分。案例在各 *_smoke.py。

跨檔引用要真的有檔案才驗得到，所以每個案例把 JSON 寫進一個暫存目錄再讀。
"""
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))   # handy 根

RESULT = {"pass": 0, "fail": 0}
TMP = tempfile.mkdtemp(prefix="handy-util-")


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


def write(name, data):
    """把 data 寫成暫存目錄下的 JSON 檔（name 可含子目錄），回絕對路徑。"""
    path = os.path.join(TMP, name)
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False)
    return path


def read_err(path):
    """讀一個預期會失敗的設定檔，回錯誤訊息（沒失敗的話回 None）。"""
    from util import config
    try:
        config.read(path)
    except Exception as e:      # noqa: BLE001 —— 測的就是它拋什麼
        return str(e)
    return None


def report():
    """印總結，回退出碼。"""
    total = RESULT["pass"] + RESULT["fail"]
    print("\n結果：%d/%d 通過" % (RESULT["pass"], total))
    return 1 if RESULT["fail"] else 0
