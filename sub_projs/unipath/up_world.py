"""unipath 執行態環境的共用常數與純函數助手（無狀態、被 up_env 與各步共用）。"""
import ast

LEAVES = ("data", "ctl", "status")   # 每個節點的 Plan 9 約定檔
CONTAINER = (list, tuple, dict)


class UnipathError(Exception):
    """帶 errno 名（如 'ENOENT'）的錯誤，跨邊界時轉成對應 errno。"""

    def __init__(self, name):
        super().__init__(name)
        self.name = name


def build_live_world():
    """示範用的活物件圖：list→數字子路徑、dict→字串鍵子路徑。"""
    return [
        0,                              # /0  counter（背景 tick 每秒 +1）
        ["alpha", "beta", "gamma"],     # /1  list
        {"name": "world", "hp": 100},   # /2  dict
    ]


def children(value):
    """一個容器的子路徑名：list→數字、dict→字串鍵；scalar→無。"""
    if isinstance(value, (list, tuple)):
        return [str(i) for i in range(len(value))]
    if isinstance(value, dict):
        return [str(k) for k in value]
    return []


def leaf_bytes(value, leaf):
    """約定檔內容：data＝值、status＝摘要、ctl＝命令說明。"""
    if leaf == "data":
        body = repr(value) if isinstance(value, CONTAINER) else str(value)
        return (body + "\n").encode()
    if leaf == "status":
        n = len(value) if isinstance(value, CONTAINER) else "-"
        return f"type={type(value).__name__} len={n} id={id(value)}\n".encode()
    if leaf == "ctl":
        return b"# append <literal> | set <key> <literal> | del <key>\n"
    raise UnipathError("ENOENT")


def parse(text):
    """把寫入的文字解成 Python 值（42 / 3.14 / 'x' / [..] / {..}），失敗則當純文字。"""
    text = text.strip()
    try:
        return ast.literal_eval(text)
    except (ValueError, SyntaxError):
        return text
