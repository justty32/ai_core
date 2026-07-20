"""config.py — handy 的共用設定讀取：讀 JSON 並解析 $env / $ref。

任何 handy 住戶都可 `from util import config` 來讀「帶指令式節點」的設定檔。
指令式節點（出現在任意欄位值的位置，就地被替換）：

  {"$env": "VAR"[, "default": …]}   取環境變數；未設（或空字串）且無 default → None（該欄位視為缺）
  {"$ref": "a.b.c"}                 引用本檔其他值（從根算 dotted 路徑）；遞迴解析、含迴圈防護

read(path) 回「完全解析後」的 dict：純資料值原樣、指令式節點換成解析結果。
"""
import json
import os


def read(path):
    """讀 JSON 設定檔、解析所有 $env/$ref，回結果 dict。檔案不存在／JSON 壞會拋。"""
    with open(path, encoding="utf-8") as f:
        root = json.load(f)
    return resolve(root, root)


def resolve(value, root, _seen=()):
    """就地解析 value 內的 $env/$ref；root＝供 $ref 查路徑的整棵設定。_seen 防 $ref 迴圈。"""
    if isinstance(value, dict):
        if "$env" in value:
            got = os.environ.get(value["$env"])
            return value.get("default") if got in (None, "") else got
        if "$ref" in value:
            path = value["$ref"]
            if path in _seen:
                raise ValueError("config $ref 迴圈：%s" % path)
            return resolve(_lookup(root, path), root, _seen + (path,))
        return {k: resolve(v, root, _seen) for k, v in value.items()}
    if isinstance(value, list):
        return [resolve(v, root, _seen) for v in value]
    return value


def _lookup(root, dotted):
    """從 root 走 dotted 路徑（a.b.c）取值；找不到就拋。"""
    cur = root
    for part in dotted.split("."):
        if not isinstance(cur, dict) or part not in cur:
            raise ValueError("config $ref 找不到路徑：%s" % dotted)
        cur = cur[part]
    return cur
