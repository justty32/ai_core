"""config.py — handy 的共用設定讀取：讀 JSON 並解析 $env / $ref。

指令式節點可放在任意欄位值的位置，read() 就地替換成解析結果：

  {"$ref": "<檔案>#<片段>"}   引用；沒有 # 就整串當檔名（引用整份檔）
  {"$ref": "#a/b/c"}          檔案部分為空＝引用本檔片段（/ 分段）
  {"$ref": ["a.json", "b.json"]}
                              依序解析後合併，**後者覆蓋前者**
  {"$env": "VAR"[, "default": …]}
                              取環境變數；未設或空且無 default → None

檔案路徑相對「當前文件所在目錄」；同一檔只讀一次（cache）。偵測到迴圈引用會拋
ValueError。引用語法本身（拆解／走路徑／合併）在 [jsref.py](jsref.py)。

⚠ $env 在變數未設時**不報錯**，回 default／None（handy 靠這個表示「該欄位沒填」）。
同一個 dict 同時有 $ref 與 $env 時，**$ref 優先**。
"""
import json
import os

from . import jsref


def read(path):
    """讀根檔並解析全部 $env/$ref，回結果 dict。讀不到／JSON 壞會拋。"""
    abs_path = os.path.abspath(path)
    r = _Resolver()
    root = r.load(abs_path)
    return r.node(root, _Doc(root, abs_path, os.path.dirname(abs_path)))


def resolve_in(js, base_dir):
    """解析已在記憶體裡的結構；base_dir＝其中相對 $ref 的基準目錄。"""
    r = _Resolver()
    return r.node(js, _Doc(js, "", base_dir))


class _Doc:
    """當前正在解析的文件：根、絕對路徑、所在目錄（相對 $ref 的基準）。"""

    def __init__(self, root, abs_path, dir_path):
        self.root = root
        self.abs = abs_path      # 記憶體入口為空字串
        self.dir = dir_path


class _Resolver:
    """一次解析作業的狀態：檔案 cache ＋ 正在解析中的引用（防迴圈）。"""

    def __init__(self):
        self.cache = {}      # 絕對路徑 → 讀進來的原始資料
        self.active = set()  # 正在解析的 "檔案#片段"

    def load(self, abs_path):
        """讀並解析一個檔案，以絕對路徑 cache。"""
        if abs_path in self.cache:
            return self.cache[abs_path]
        with open(abs_path, encoding="utf-8") as f:
            data = json.load(f)
        self.cache[abs_path] = data
        return data

    def node(self, value, doc):
        """遞迴走訪任意 JSON 值，回展開後的新值。"""
        if isinstance(value, dict):
            if "$ref" in value:
                return self.ref(value["$ref"], doc)
            if "$env" in value:
                got = os.environ.get(value["$env"])
                if got in (None, ""):
                    return value.get("default")
                return got
            return {k: self.node(v, doc) for k, v in value.items()}
        if isinstance(value, list):
            return [self.node(v, doc) for v in value]
        return value

    def ref(self, raw, doc):
        """$ref 的值：字串＝單一引用；陣列＝依序引用後合併（後者覆蓋前者）。"""
        if isinstance(raw, str):
            return self.one(raw, doc)
        if not isinstance(raw, list):
            raise ValueError("config $ref 要字串或字串陣列，給了 %s"
                             % type(raw).__name__)
        if not raw:
            raise ValueError("config $ref 陣列不可為空")
        merged = None
        for i, item in enumerate(raw):
            if not isinstance(item, str):
                raise ValueError("config $ref 陣列第 %d 項要字串，給了 %s"
                                 % (i, type(item).__name__))
            value = self.one(item, doc)
            merged = value if i == 0 else jsref.merge(merged, value)
        return merged

    def one(self, ref, doc):
        """解析單一 $ref 字串。"""
        file, frag = jsref.split_ref(ref)
        if file:
            target = file
            if not os.path.isabs(file):
                target = os.path.join(doc.dir, file)
            target = os.path.normpath(target)
            root = self.load(target)
            next_doc = _Doc(root, target, os.path.dirname(target))
        else:
            root, target, next_doc = doc.root, doc.abs, doc

        key = target + "#" + frag
        if key in self.active:
            raise ValueError("config $ref 迴圈：%s" % key)
        value, found = jsref.lookup(root, frag)
        if not found:
            raise ValueError("config $ref 找不到路徑：%s（在 %s）"
                             % (ref, target or "<記憶體>"))
        self.active.add(key)
        try:
            return self.node(value, next_doc)
        finally:
            self.active.discard(key)
