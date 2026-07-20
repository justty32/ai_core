"""argv.py — 命令列掃描：把 argv 拆成旗標與位置參數（對齊 cli.cpp 的解析段）。

固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
特判；反射旗標（連線／取樣，對應 Client 欄位，見 flags.py）吃下一個 argv；其餘當位置參數拼
prompt。「-」單獨＝stdin 插入點，其餘 '-' 開頭＝未知旗標。掃描結果收成 ParsedArgs；遇 --help
或用法錯時回一個退出碼交呼叫端直接返回（main 只讀結果、不重演解析）。
"""
import sys

from .flags import FLAG_TO_FIELD, print_usage
from .internal import EXIT_OK, EXIT_USAGE


class ParsedArgs:
    """argv 掃描結果：旗標收成的各欄位，交給 cli.main 續組 prompt／config／請求。"""

    def __init__(self):
        self.raw_values = {}       # 反射欄位名 → (原始字串, 型別, flag)
        self.prompt_parts = []
        self.media_specs = []
        self.tool_specs = []
        self.modality_specs = []
        self.schema_text = None
        self.config_path = None
        self.media_out_dir = None
        self.system_text = None
        self.has_schema = False
        self.has_config = False
        self.has_system = False
        self.stream = False


def parse_argv(args):
    """掃描 argv，回 (ParsedArgs, None)；遇 --help／用法錯回 (None, 退出碼)。"""
    p = ParsedArgs()
    i = 1
    n = len(args)

    def need_value(flag):
        """吃下一個 argv 當旗標的值；缺值回 None（呼叫端據此回 EXIT_USAGE）。"""
        nonlocal i
        if i + 1 >= n:
            sys.stderr.write("%s 缺少值（llm --help 看用法）\n" % flag)
            return None
        i += 1
        return args[i]

    no_more_flags = False
    while i < n:
        a = args[i]
        if no_more_flags:
            p.prompt_parts.append(a)
            i += 1
            continue
        if a == "--":
            no_more_flags = True
            i += 1
            continue
        if a in ("--help", "-h"):
            print_usage()
            return None, EXIT_OK
        if a == "--stream":
            p.stream = True
            i += 1
            continue
        if a in ("--image", "--media"):
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.media_specs.append(v)
            i += 1
            continue
        if a == "--schema":
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.schema_text, p.has_schema = v, True
            i += 1
            continue
        if a == "--system":
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.system_text, p.has_system = v, True
            i += 1
            continue
        if a == "--config":
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.config_path, p.has_config = v, True
            i += 1
            continue
        if a == "--tool":
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.tool_specs.append(v)
            i += 1
            continue
        if a == "--modality":
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.modality_specs.append(v)
            i += 1
            continue
        if a == "--media-out":
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            p.media_out_dir = v
            i += 1
            continue
        if a in FLAG_TO_FIELD:
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            field, typ = FLAG_TO_FIELD[a]
            p.raw_values[field] = (v, typ, a)
            i += 1
            continue
        if a and a[0] == "-" and a != "-":  # 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標
            sys.stderr.write("未知旗標：%s（llm --help 看用法）\n" % a)
            return None, EXIT_USAGE
        p.prompt_parts.append(a)
        i += 1

    return p, None
