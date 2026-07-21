"""config_smoke.py — util.config／util.jsref 的離線測試（不連網、零相依）。

跑法：python util/test/config_smoke.py（從哪跑都行）。全過回 0，有失敗回 1。
每個案例把 JSON 寫進暫存目錄再讀，跨檔引用才驗得到。
"""
import json
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))   # handy 根

from util import config, jsref    # noqa: E402

RESULT = {"pass": 0, "fail": 0}
TMP = tempfile.mkdtemp(prefix="handy-config-")


def check(desc, got, want):
    if got == want:
        print("  PASS  " + desc)
        RESULT["pass"] += 1
        return
    print("  FAIL  " + desc)
    print("        got =%r" % (got,))
    print("        want=%r" % (want,))
    RESULT["fail"] += 1


def write(name, data):
    """把 data 寫成 TMP 下的 JSON 檔（name 可含子目錄），回絕對路徑。"""
    path = os.path.join(TMP, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False)
    return path


def read_err(path):
    """讀一個預期會失敗的檔，回錯誤訊息（成功的話回 None）。"""
    try:
        config.read(path)
    except Exception as e:      # noqa: BLE001 —— 測的就是它拋什麼
        return str(e)
    return None


def test_env():
    print("== $env ==")
    os.environ["CFG_A"] = "值"
    os.environ["CFG_EMPTY"] = ""
    os.environ.pop("CFG_MISSING", None)
    p = write("env.json", {
        "a": {"$env": "CFG_A"},
        "missing": {"$env": "CFG_MISSING"},
        "fallback": {"$env": "CFG_MISSING", "default": "預設"},
        "empty": {"$env": "CFG_EMPTY", "default": "預設"}})
    got = config.read(p)
    check("有設就用該值", got["a"], "值")
    check("未設且無 default → None（鍵仍在）", got["missing"], None)
    check("未設有 default", got["fallback"], "預設")
    check("設成空字串也走 default", got["empty"], "預設")


def test_fragment():
    print("== 本檔片段（#a/b/c）==")
    p = write("frag.json", {
        "shared": {"url": "http://x", "deep": {"n": 7}},
        "list": ["零", "壹"],
        "whole": {"$ref": "#shared"},
        "deep": {"$ref": "#shared/deep/n"},
        "item": {"$ref": "#list/1"},
        "slash": {"$ref": "#/shared/url"}})
    got = config.read(p)
    check("引用整個節點", got["whole"], {"url": "http://x", "deep": {"n": 7}})
    check("走巢狀路徑", got["deep"], 7)
    check("數字段索引陣列", got["item"], "壹")
    check("片段開頭的 / 可有可無", got["slash"], "http://x")


def test_cross_file():
    print("== 跨檔引用 ==")
    write("base.json", {"host": "http://base", "opt": {"a": 1, "b": 2}})
    write("sub/inner.json", {"who": "inner"})
    p = write("cross.json", {
        "all": {"$ref": "base.json"},
        "part": {"$ref": "base.json#opt/a"},
        "nested": {"$ref": "sub/inner.json#who"}})
    got = config.read(p)
    check("引用整份檔", got["all"],
          {"host": "http://base", "opt": {"a": 1, "b": 2}})
    check("引用他檔片段", got["part"], 1)
    check("相對路徑含子目錄", got["nested"], "inner")


def test_list_merge():
    print("== $ref 陣列：依序合併、後者覆蓋前者 ==")
    write("m1.json", {"a": 1, "b": {"x": 1, "y": 1}, "keep": "留著"})
    write("m2.json", {"a": 2, "b": {"y": 9, "z": 9}, "new": "新的"})
    p = write("merge.json", {
        "merged": {"$ref": ["m1.json", "m2.json"]},
        "reversed": {"$ref": ["m2.json", "m1.json"]},
        "single": {"$ref": ["m1.json"]}})
    got = config.read(p)
    check("後者覆蓋前者", got["merged"]["a"], 2)
    check("前者獨有的保留", got["merged"]["keep"], "留著")
    check("後者獨有的加入", got["merged"]["new"], "新的")
    check("兩邊都是 object → 遞迴深合併",
          got["merged"]["b"], {"x": 1, "y": 9, "z": 9})
    check("順序反過來就換前者贏", got["reversed"]["a"], 1)
    check("只有一個元素也行", got["single"]["a"], 1)

    check("非 dict 由後者整個取代", jsref.merge({"a": 1}, [1, 2]), [1, 2])
    check("陣列不深合併", jsref.merge([1, 2, 3], [9]), [9])


def test_errors():
    print("== 錯誤 ==")
    check("陣列不可為空",
          read_err(write("e1.json", {"k": {"$ref": []}})),
          "config $ref 陣列不可為空")
    check("陣列元素要字串",
          read_err(write("e2.json", {"k": {"$ref": [1]}})),
          "config $ref 陣列第 0 項要字串，給了 int")
    check("$ref 型別不對",
          read_err(write("e3.json", {"k": {"$ref": 1}})),
          "config $ref 要字串或字串陣列，給了 int")
    err = read_err(write("e4.json", {"a": {"$ref": "#b"}, "b": {"$ref": "#a"}}))
    check("迴圈引用", err.startswith("config $ref 迴圈"), True)
    err = read_err(write("e5.json", {"k": {"$ref": "#no/such"}}))
    check("片段不存在", err.startswith("config $ref 找不到路徑"), True)


if __name__ == "__main__":
    try:
        test_env()
        test_fragment()
        test_cross_file()
        test_list_merge()
        test_errors()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    total = RESULT["pass"] + RESULT["fail"]
    print("\n結果：%d/%d 通過" % (RESULT["pass"], total))
    sys.exit(1 if RESULT["fail"] else 0)
