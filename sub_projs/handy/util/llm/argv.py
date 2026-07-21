"""argv.py — 命令列掃描：把 argv 拆成旗標與位置參數。

沿用已封存 pllm 的同名檔。固定旗標（--stream／--image／--schema／--system／
--config／--tool／--modality／--media-out／--help／--）逐項特判；反射旗標（連線／
取樣，見 flags.py）吃下一個 argv；其餘當位置參數拼 prompt。「-」單獨＝stdin 插入點，
其餘 '-' 開頭＝未知旗標。遇 --help 或用法錯就回一個退出碼，交呼叫端直接返回。
"""
import sys

from .errors import EXIT_OK, EXIT_USAGE
from .flags import FLAG_TO_FIELD, print_usage


class ParsedArgs:
    """argv 掃描結果：交給 cli.main 續組 prompt／config／請求。"""

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

        if a in ("--image", "--media", "--schema", "--system", "--config",
                 "--tool", "--modality", "--media-out") or a in FLAG_TO_FIELD:
            v = need_value(a)
            if v is None:
                return None, EXIT_USAGE
            if a in ("--image", "--media"):
                p.media_specs.append(v)
            elif a == "--tool":
                p.tool_specs.append(v)
            elif a == "--modality":
                p.modality_specs.append(v)
            elif a == "--schema":
                p.schema_text = v
                p.has_schema = True
            elif a == "--system":
                p.system_text = v
                p.has_system = True
            elif a == "--config":
                p.config_path = v
                p.has_config = True
            elif a == "--media-out":
                p.media_out_dir = v
            else:
                field, typ = FLAG_TO_FIELD[a]
                p.raw_values[field] = (v, typ, a)
            i += 1
            continue

        if a and a[0] == "-" and a != "-":   # 「-」＝stdin 佔位符
            sys.stderr.write("未知旗標：%s（llm --help 看用法）\n" % a)
            return None, EXIT_USAGE

        p.prompt_parts.append(a)
        i += 1

    return p, None
