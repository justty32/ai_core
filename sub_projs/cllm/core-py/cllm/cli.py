"""cli.py — 薄 CLI 外殼：把命令列組成一次 cllm.core.ask 的發問（鏡像 C++ 的 `llm` CLI）。

只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給內核 core.ask。周邊拆到
同套件的姊妹模組（皆對齊 C++ 的分檔）：
  internal.py（cli_internal.hpp）＝退出碼／env 鍵／檔案讀取；flags.py（cli_flags.cpp）＝反射旗標表
  ＋print_usage；argv.py＝argv 掃描（cli.cpp 解析段）；config.py（cli_config.cpp）＝三層 config
  解析；media.py＝--media 三分流取值；reqinput.py＝請求類旗標組成 core.ask 輸入（cli.cpp 組請求段）；
  output.py（cli_output.cpp）＝出口 handlers（Sink）。本檔（對齊 cli.cpp）只留 main 接線。

流程（對齊 cli.cpp）：
  (1) 掃描 argv（argv.parse_argv）：固定旗標特判、反射旗標吃下一 argv、其餘拼 prompt。
  (2) 定 prompt：位置參數與導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有＝
      prompt＋空行＋stdin；只給其一＝用那一個；互動終端不讀 stdin（避免卡住）。
  (3) 組 client 設定：內建預設 → config 檔（--config ＞ 環境變數 LLM_CLI_CONFIG ＞
      ~/.config/llm/config.json）→ 反射旗標覆寫。
  (4) 組請求輸入（reqinput.build_request_inputs）＋呼叫 core.ask，output 走 Sink：文字吐
      stdout、tool_calls 一行一則 JSON、媒體落檔 --media-out、錯誤吐 stderr。

退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯（未知旗標／缺 prompt／檔案讀不到／config 壞／
數值型別錯）；2 請求失敗（傳輸／後端／媒體落檔失敗）；130 SIGINT 取消。
"""
import sys

from . import core
from .argv import parse_argv
from .config import load_config
from .internal import EXIT_CANCEL, EXIT_OK, EXIT_REQUEST, EXIT_USAGE
from .output import Sink
from .reqinput import build_request_inputs


def main(argv=None):
    """CLI 進入點；回退出碼。"""
    args = list(sys.argv if argv is None else argv)

    # ── (1) 掃描 argv ──
    p, ec = parse_argv(args)
    if p is None:
        return ec

    # ── (2) prompt：位置參數與導管 stdin 可合體 ──
    has_dash = any(part == "-" for part in p.prompt_parts)
    stdin_text = ""
    if not sys.stdin.isatty():  # 只在被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）
        stdin_text = sys.stdin.read().rstrip("\r\n")  # 去尾端換行，避免多餘空白進 prompt
    elif has_dash:
        sys.stderr.write("「-」要從 stdin 讀，但 stdin 是互動終端"
                         "——用導管/檔案餵入（llm --help 看用法）\n")
        return EXIT_USAGE

    pieces = [stdin_text if part == "-" else part for part in p.prompt_parts]
    prompt = " ".join(pieces)
    if not has_dash and stdin_text:  # 沒寫「-」而兩者都有＝prompt＋空行＋stdin
        prompt = stdin_text if not prompt else prompt + "\n\n" + stdin_text
    if not prompt:
        sys.stderr.write("缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）\n")
        return EXIT_USAGE

    # ── (3) 組 client 設定：內建預設 → config → 反射旗標覆寫 ──
    client = {}
    ec = load_config(client, p.has_config, p.config_path)
    if ec != EXIT_OK:
        return ec
    for field, (val, typ, flag) in p.raw_values.items():
        try:
            client[field] = typ(val)
        except (TypeError, ValueError):
            kind = {int: "整數", float: "小數", str: "字串"}[typ]
            sys.stderr.write("%s 需要%s（給了「%s」）\n" % (flag, kind, val))
            return EXIT_USAGE

    # ── (4) 組請求輸入 ＋ 呼叫內核 ──
    r, ec = build_request_inputs(p)
    if r is None:
        return ec

    sink = Sink(p.media_out_dir)
    core.ask(prompt, client.get("endpoint"),
             system=p.system_text if p.has_system else None,
             api_key=client.get("api_key"), model=client.get("model"),
             timeout_ms=client.get("timeout_ms"),
             temperature=client.get("temperature"), top_p=client.get("top_p"),
             presence_penalty=client.get("presence_penalty"),
             frequency_penalty=client.get("frequency_penalty"),
             max_tokens=client.get("max_tokens"), seed=client.get("seed"),
             stream=p.stream, schema=r.schema_body, tools=r.tool_defs or None,
             media=r.media_items or None, modalities=r.modality_items or None,
             on_delta=sink.on_delta, on_tool=sink.on_tool,
             on_media=sink.on_media, on_error=sink.on_error)

    ok = not sink.had_error
    if sink.wrote_text and ok:  # 補尾端換行（stdout 收尾乾淨）
        sys.stdout.write("\n")
        sys.stdout.flush()
    if not ok:
        return EXIT_REQUEST
    return EXIT_REQUEST if sink.media_err else EXIT_OK  # 媒體落檔失敗＝結果真掉了


def _entry():
    """console_scripts 進入點：包 KeyboardInterrupt → 130（對齊 SIGINT 取消）。"""
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.stderr.write("\n已取消\n")
        sys.exit(EXIT_CANCEL)


if __name__ == "__main__":
    _entry()
