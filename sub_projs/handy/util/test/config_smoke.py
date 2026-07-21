"""config_smoke.py — util.config／util.jsref 的離線測試（不連網、零相依）。

跑法：python util/test/config_smoke.py（從哪跑都行）。全過回 0，有失敗回 1。
管線（斷言／寫暫存 JSON／記分）在 harness.py，本檔只放案例。
"""
import os
import shutil
import sys

from harness import (TMP, check, read_err,      # noqa: E402
                     report, write)

from util import config, jsref                  # noqa: E402


def test_env():
    print("== $env ==")
    os.environ["CFG_A"] = "值"
    os.environ["CFG_EMPTY"] = ""
    os.environ.pop("CFG_MISSING", None)
    got = config.read(write("env.json", {
        "a": {"$env": "CFG_A"},
        "missing": {"$env": "CFG_MISSING"},
        "fallback": {"$env": "CFG_MISSING", "default": "預設"},
        "empty": {"$env": "CFG_EMPTY", "default": "預設"}}))
    check("有設就用該值", got["a"], "值")
    check("未設且無 default → None（鍵仍在）", got["missing"], None)
    check("未設有 default", got["fallback"], "預設")
    check("設成空字串也走 default", got["empty"], "預設")


def test_fragment():
    print("== 本檔片段（#a/b/c）==")
    got = config.read(write("frag.json", {
        "shared": {"url": "http://x", "deep": {"n": 7}},
        "list": ["零", "壹"],
        "whole": {"$ref": "#shared"},
        "deep": {"$ref": "#shared/deep/n"},
        "item": {"$ref": "#list/1"},
        "slash": {"$ref": "#/shared/url"}}))
    check("引用整個節點", got["whole"], {"url": "http://x", "deep": {"n": 7}})
    check("走巢狀路徑", got["deep"], 7)
    check("數字段索引陣列", got["item"], "壹")
    check("片段開頭的 / 可有可無", got["slash"], "http://x")


def test_cross_file():
    print("== 跨檔引用 ==")
    write("base.json", {"host": "http://base", "opt": {"a": 1, "b": 2}})
    write("sub/inner.json", {"who": "inner"})
    got = config.read(write("cross.json", {
        "all": {"$ref": "base.json"},
        "part": {"$ref": "base.json#opt/a"},
        "nested": {"$ref": "sub/inner.json#who"}}))
    check("引用整份檔", got["all"],
          {"host": "http://base", "opt": {"a": 1, "b": 2}})
    check("引用他檔片段", got["part"], 1)
    check("相對路徑含子目錄", got["nested"], "inner")


def test_list_merge():
    print("== $ref 陣列：依序合併、後者覆蓋前者 ==")
    write("m1.json", {"a": 1, "b": {"x": 1, "y": 1}, "keep": "留著"})
    write("m2.json", {"a": 2, "b": {"y": 9, "z": 9}, "new": "新的"})
    got = config.read(write("merge.json", {
        "merged": {"$ref": ["m1.json", "m2.json"]},
        "reversed": {"$ref": ["m2.json", "m1.json"]},
        "single": {"$ref": ["m1.json"]}}))
    check("後者覆蓋前者", got["merged"]["a"], 2)
    check("前者獨有的保留", got["merged"]["keep"], "留著")
    check("後者獨有的加入", got["merged"]["new"], "新的")
    check("兩邊都是 object → 遞迴深合併",
          got["merged"]["b"], {"x": 1, "y": 9, "z": 9})
    check("順序反過來就換前者贏", got["reversed"]["a"], 1)
    check("只有一個元素也行", got["single"]["a"], 1)
    check("非 dict 由後者整個取代", jsref.merge({"a": 1}, [1, 2]), [1, 2])
    check("陣列不深合併", jsref.merge([1, 2, 3], [9]), [9])


def test_cycle():
    """迴圈偵測：鍵是「絕對路徑#片段」，涵蓋同檔與跨檔。"""
    print("== 迴圈偵測 ==")
    write("ca.json", {"x": {"$ref": "cb.json#y"}})
    write("cb.json", {"y": {"$ref": "ca.json#x"}})
    write("la.json", {"v": {"$ref": "lb.json#v"}})
    write("lb.json", {"v": {"$ref": "la.json#v"}})
    write("self.json", {"k": {"$ref": "self.json"}})
    cases = [
        ("同檔互相引用", write("cyc.json", {"a": {"$ref": "#b"},
                                            "b": {"$ref": "#a"}})),
        ("跨檔互相引用", os.path.join(TMP, "ca.json")),
        ("自己引用整份自己", os.path.join(TMP, "self.json")),
        ("陣列形式裡有迴圈",
         write("lst.json", {"k": {"$ref": ["la.json", "lb.json"]}})),
    ]
    for desc, path in cases:
        err = read_err(path) or ""
        check(desc, err.startswith("config $ref 迴圈"), True)


def test_not_cycle():
    """菱形／重複引用不該被誤判：解完就從 active 移除，只有繞回自己才算。"""
    print("== 不該誤判成迴圈 ==")
    write("dbase.json", {"n": 1})
    got = config.read(write("dia.json", {"a": {"$ref": "dbase.json#n"},
                                         "b": {"$ref": "dbase.json#n"}}))
    check("同一檔被引用兩次", got, {"a": 1, "b": 1})
    got = config.read(write("dia2.json", {"s": {"m": 5},
                                          "a": {"$ref": "#s"},
                                          "b": {"$ref": "#s"}}))
    check("同一片段前後引用兩次", [got["a"], got["b"]], [{"m": 5}, {"m": 5}])
    got = config.read(write("dia3.json",
                            {"k": {"$ref": ["dbase.json", "dbase.json"]}}))
    check("陣列引用同一檔兩次", got["k"], {"n": 1})


def test_errors():
    print("== 錯誤 ==")
    bad = [("dict 當 $ref", {"$ref": {"a": 1}}, "給了 dict"),
           ("null 當 $ref", {"$ref": None}, "給了 NoneType"),
           ("數字當 $ref", {"$ref": 1}, "給了 int")]
    for desc, node, tail in bad:
        check(desc, read_err(write("e.json", {"k": node})),
              "config $ref 要字串或字串陣列，" + tail)
    check("陣列不可為空",
          read_err(write("e1.json", {"k": {"$ref": []}})),
          "config $ref 陣列不可為空")
    check("陣列元素要字串",
          read_err(write("e2.json", {"k": {"$ref": [1]}})),
          "config $ref 陣列第 0 項要字串，給了 int")
    check("陣列元素是陣列也不行",
          read_err(write("e3.json", {"k": {"$ref": [["a.json"]]}})),
          "config $ref 陣列第 0 項要字串，給了 list")
    err = read_err(write("e4.json", {"k": {"$ref": "#no/such"}})) or ""
    check("片段不存在", err.startswith("config $ref 找不到路徑"), True)


if __name__ == "__main__":
    try:
        test_env()
        test_fragment()
        test_cross_file()
        test_list_merge()
        test_cycle()
        test_not_cycle()
        test_errors()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    sys.exit(report())
