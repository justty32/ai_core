"""cli.py — 薄 CLI 外殼：把命令列組成一次 core.ask 的發問。

沿用已封存 pllm 的同名檔，只做「參數解析＋I/O 接線」，真正的活全丟給 core.ask。
周邊拆在姊妹模組：flags＝旗標表＋用法、argv＝掃描、cfg＝config 檔、req＝請求類旗標、
media＝--media 取值、out＝出口 Sink。

流程：
  (1) 掃描 argv。
  (2) 定 prompt：位置參數與導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者
      都有＝prompt＋空行＋stdin；只給其一＝用那一個；互動終端不讀 stdin。
  (3) 組設定：內建預設 → config 檔 → 反射旗標覆寫。
  (4) 組請求輸入＋呼叫 core.ask，輸出走 Sink。

退出碼：0 成功；1 用法錯；2 請求失敗；130 取消。
"""
import sys

from . import core
from .argv import parse_argv
from .cfg import load_config
from .errors import EXIT_CANCEL, EXIT_OK, EXIT_REQUEST, EXIT_USAGE
from .out import Sink
from .req import build_request_inputs


def _read_stdin():
    """被導管／檔案餵入時整段讀（互動終端不讀，避免卡住）。"""
    if sys.stdin.isatty():
        return ""
    return sys.stdin.read().rstrip("\r\n")   # 去尾端換行，免多餘空白進 prompt


def _make_prompt(parts, stdin_text):
    """位置參數與 stdin 合體成一段 prompt。"""
    has_dash = "-" in parts
    pieces = []
    for part in parts:
        if part == "-":
            pieces.append(stdin_text)
        else:
            pieces.append(part)
    prompt = " ".join(pieces)
    if has_dash or not stdin_text:
        return prompt
    if not prompt:                            # 只有 stdin
        return stdin_text
    return prompt + "\n\n" + stdin_text       # 指令在前、資料在後


def _apply_flags(client, raw_values):
    """反射旗標覆寫 config；型別轉不動＝用法錯。回退出碼。"""
    for field, item in raw_values.items():
        val, typ, flag = item
        try:
            client[field] = typ(val)
        except (TypeError, ValueError):
            kind = {int: "整數", float: "小數", str: "字串"}[typ]
            sys.stderr.write("%s 需要%s（給了「%s」）\n" % (flag, kind, val))
            return EXIT_USAGE
    return EXIT_OK


def main(argv=None):
    """CLI 進入點；回退出碼。

    argv **不含程式名**——`main(["--stream", "你好"])` 即可，不必墊一個假的第 0 項。
    省略則取 `sys.argv[1:]`。⚠ 這是與已封存 pllm 的分歧（那邊收整份 sys.argv）。
    """
    args = list(sys.argv[1:] if argv is None else argv)

    p, ec = parse_argv(args)
    if p is None:
        return ec

    stdin_text = _read_stdin()
    if not stdin_text and "-" in p.prompt_parts and sys.stdin.isatty():
        sys.stderr.write("「-」要從 stdin 讀，但 stdin 是互動終端"
                         "——用導管／檔案餵入（llm --help 看用法）\n")
        return EXIT_USAGE
    prompt = _make_prompt(p.prompt_parts, stdin_text)
    if not prompt:
        sys.stderr.write("缺少 prompt：給位置參數或從 stdin 餵入"
                         "（llm --help 看用法）\n")
        return EXIT_USAGE

    client = {}
    ec = load_config(client, p.has_config, p.config_path)
    if ec != EXIT_OK:
        return ec
    ec = _apply_flags(client, p.raw_values)
    if ec != EXIT_OK:
        return ec

    r, ec = build_request_inputs(p)
    if r is None:
        return ec

    system = None
    if p.has_system:
        system = p.system_text
    sink = Sink(p.media_out_dir)
    core.ask(prompt, client.get("endpoint"), system=system,
             api_key=client.get("api_key"), model=client.get("model"),
             timeout_ms=client.get("timeout_ms"),
             temperature=client.get("temperature"),
             top_p=client.get("top_p"),
             presence_penalty=client.get("presence_penalty"),
             frequency_penalty=client.get("frequency_penalty"),
             max_tokens=client.get("max_tokens"), seed=client.get("seed"),
             stream=p.stream, schema=r.schema_body,
             tools=r.tool_defs or None, media=r.media_items or None,
             modalities=r.modality_items or None,
             on_delta=sink.on_delta, on_tool=sink.on_tool,
             on_media=sink.on_media, on_error=sink.on_error)

    if sink.wrote_text and not sink.had_error:   # 補尾端換行，stdout 收尾乾淨
        sys.stdout.write("\n")
        sys.stdout.flush()
    if sink.had_error:
        return EXIT_REQUEST
    if sink.media_err:                           # 媒體落檔失敗＝結果真掉了
        return EXIT_REQUEST
    return EXIT_OK


def _entry():
    """裝成指令時的進入點：包 KeyboardInterrupt → 130。"""
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.stderr.write("\n已取消\n")
        sys.exit(EXIT_CANCEL)


if __name__ == "__main__":
    _entry()
