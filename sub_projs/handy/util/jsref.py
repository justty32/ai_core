"""jsref.py — JSON `$ref` 語法的純函式：拆引用、走片段路徑、合併。

不碰檔案、不遞迴解析——那些在 [config.py](config.py)。這裡只有三個純轉換，
語意對齊 guanyu 專案的 util_jsonschema.go。
"""


def split_ref(ref):
    """把「<檔案>#<片段>」拆成 (檔案, 片段)；沒有 # 就整串是檔案。

    「#a/b」→ ("", "a/b")＝本檔片段；「x.json」→ ("x.json", "")＝整份檔。
    """
    if "#" not in ref:
        return ref, ""
    file, _, frag = ref.partition("#")
    return file, frag.removeprefix("/")


def lookup(doc, frag):
    """沿片段路徑（/ 分段）取值，回 (值, 有沒有找到)。

    空路徑＝整份文件。數字段可索引陣列。回「有沒有找到」而非直接拋，
    是為了分辨「路徑不存在」與「值本來就是 null」。
    """
    if frag == "":
        return doc, True
    cur = doc
    for seg in frag.split("/"):
        if isinstance(cur, dict):
            if seg not in cur:
                return None, False
            cur = cur[seg]
        elif isinstance(cur, list):
            if not seg.isdigit() or int(seg) >= len(cur):
                return None, False
            cur = cur[int(seg)]
        else:
            return None, False
    return cur, True


def merge(a, b):
    """合併兩個已解析的值，衝突以 b（後者）為準。

    只有兩邊都是 dict 才遞迴深合併——後者只覆蓋它指定的鍵，其餘保留前者。
    任一邊不是 dict（含陣列、純量）就整個用 b 取代。
    """
    if not isinstance(a, dict) or not isinstance(b, dict):
        return b
    out = dict(a)
    for k, bv in b.items():
        if k in out:
            out[k] = merge(out[k], bv)
        else:
            out[k] = bv
    return out
