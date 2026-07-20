"""cli.py — 薄 CLI 外殼：把命令列組成一次 cllm.core.ask 的發問（鏡像 C++ 的 `llm` CLI）。

只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給內核 core.ask。周邊拆到
同套件的姊妹模組（皆對齊 C++ 的分檔）：
  internal.py（cli_internal.hpp）＝退出碼／env 鍵／檔案讀取；flags.py（cli_flags.cpp）＝反射旗標表
  ＋print_usage；config.py（cli_config.cpp）＝三層 config 解析；media.py＝--media 三分流取值；
  output.py（cli_output.cpp）＝出口 handlers（Sink）。本檔（對齊 cli.cpp）只留 main 解析＋接線。

流程（對齊 cli.cpp）：
  (1) 解析 argv：固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/
      --media-out/--help/--）特判；反射旗標（連線／取樣，對應 Client 欄位）吃下一個 argv；
      其餘當位置參數拼 prompt。「-」單獨＝stdin 插入點，其餘 '-' 開頭＝未知旗標。
  (2) 定 prompt：位置參數與導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有＝
      prompt＋空行＋stdin；只給其一＝用那一個；互動終端不讀 stdin（避免卡住）。
  (3) 組 client 設定：內建預設 → config 檔（--config ＞ 環境變數 LLM_CLI_CONFIG ＞
      ~/.config/llm/config.json）→ 反射旗標覆寫。
  (4) 組請求＋呼叫 core.ask，output 走 Sink：文字吐 stdout、tool_calls 一行一則 JSON、
      媒體落檔 --media-out、錯誤吐 stderr。

退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯（未知旗標／缺 prompt／檔案讀不到／config 壞／
數值型別錯）；2 請求失敗（傳輸／後端／媒體落檔失敗）；130 SIGINT 取消。
"""
import json
import os
import sys

from . import core
from .config import load_config
from .flags import FLAG_TO_FIELD, print_usage
from .internal import EXIT_CANCEL, EXIT_OK, EXIT_REQUEST, EXIT_USAGE
from .media import build_media_item
from .output import Sink


def main(argv=None):
    """CLI 進入點；回退出碼。"""
    args = list(sys.argv if argv is None else argv)

    raw_values = {}       # 反射欄位名 → (原始字串, 型別, flag)
    prompt_parts = []
    media_specs, tool_specs, modality_specs = [], [], []
    schema_text = config_path = media_out_dir = system_text = None
    has_schema = has_config = has_system = False
    stream = False
    no_more_flags = False

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

    while i < n:
        a = args[i]
        if no_more_flags:
            prompt_parts.append(a)
            i += 1
            continue
        if a == "--":
            no_more_flags = True
            i += 1
            continue
        if a in ("--help", "-h"):
            print_usage()
            return EXIT_OK
        if a == "--stream":
            stream = True
            i += 1
            continue
        if a in ("--image", "--media"):
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            media_specs.append(v)
            i += 1
            continue
        if a == "--schema":
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            schema_text, has_schema = v, True
            i += 1
            continue
        if a == "--system":
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            system_text, has_system = v, True
            i += 1
            continue
        if a == "--config":
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            config_path, has_config = v, True
            i += 1
            continue
        if a == "--tool":
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            tool_specs.append(v)
            i += 1
            continue
        if a == "--modality":
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            modality_specs.append(v)
            i += 1
            continue
        if a == "--media-out":
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            media_out_dir = v
            i += 1
            continue
        if a in FLAG_TO_FIELD:
            v = need_value(a)
            if v is None:
                return EXIT_USAGE
            field, typ = FLAG_TO_FIELD[a]
            raw_values[field] = (v, typ, a)
            i += 1
            continue
        if a and a[0] == "-" and a != "-":  # 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標
            sys.stderr.write("未知旗標：%s（llm --help 看用法）\n" % a)
            return EXIT_USAGE
        prompt_parts.append(a)
        i += 1

    # ── (2) prompt：位置參數與導管 stdin 可合體 ──
    has_dash = any(p == "-" for p in prompt_parts)
    stdin_text = ""
    if not sys.stdin.isatty():  # 只在被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）
        stdin_text = sys.stdin.read().rstrip("\r\n")  # 去尾端換行，避免多餘空白進 prompt
    elif has_dash:
        sys.stderr.write("「-」要從 stdin 讀，但 stdin 是互動終端"
                         "——用導管/檔案餵入（llm --help 看用法）\n")
        return EXIT_USAGE

    pieces = [stdin_text if p == "-" else p for p in prompt_parts]
    prompt = " ".join(pieces)
    if not has_dash and stdin_text:  # 沒寫「-」而兩者都有＝prompt＋空行＋stdin
        prompt = stdin_text if not prompt else prompt + "\n\n" + stdin_text
    if not prompt:
        sys.stderr.write("缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）\n")
        return EXIT_USAGE

    # ── (3) 組 client 設定：內建預設 → config → 反射旗標覆寫 ──
    client = {}
    ec = load_config(client, has_config, config_path)
    if ec != EXIT_OK:
        return ec
    for field, (val, typ, flag) in raw_values.items():
        try:
            client[field] = typ(val)
        except (TypeError, ValueError):
            kind = {int: "整數", float: "小數", str: "字串"}[typ]
            sys.stderr.write("%s 需要%s（給了「%s」）\n" % (flag, kind, val))
            return EXIT_USAGE

    # ── (4) 組請求輸入 ──
    # ⚠ 與 C++ llm 刻意分歧（只發生在 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON 文字」
    #    （同 --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
    schema_body = None
    if has_schema:
        try:
            json.loads(schema_text)  # 只驗合法；字面原樣交內核嵌入
        except Exception:
            sys.stderr.write("--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）\n")
            return EXIT_USAGE
        schema_body = schema_text

    tool_defs = []
    for spec in tool_specs:  # 字面 tool def JSON；需 name 與非空 parameters（對齊 load_tool_def 語意）
        try:
            td = json.loads(spec)
        except Exception:
            sys.stderr.write("--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）\n")
            return EXIT_USAGE
        if not isinstance(td, dict) or not td.get("name") or not td.get("parameters"):
            sys.stderr.write("--tool 缺 name 或 parameters\n")
            return EXIT_USAGE
        tool_defs.append({"name": td["name"], "description": td.get("description", ""),
                          "parameters": td["parameters"]})

    modality_items = []
    for spec in modality_specs:  # 「名」或「名=<字面JSON>」
        name, eq, cfg = spec.partition("=")
        if not name:
            sys.stderr.write("--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）\n")
            return EXIT_USAGE
        if eq:  # 有 '='：cfg 收字面 JSON，驗合法
            try:
                json.loads(cfg)
            except Exception:
                sys.stderr.write("--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）\n")
                return EXIT_USAGE
        modality_items.append({"name": name, "config": cfg if eq else None})

    media_items = []
    for spec in media_specs:  # 三分流：URL 直通 / .json 描述子直通 / 二進位圖檔讀檔編碼
        item = build_media_item(spec)
        if item is None:
            return EXIT_USAGE
        media_items.append(item)

    if media_out_dir and not os.path.isdir(media_out_dir):
        sys.stderr.write("--media-out 不是可用目錄：%s\n" % media_out_dir)
        return EXIT_USAGE

    # ── 出口 handlers（Sink，見 output.py）＋ 呼叫內核 ──
    sink = Sink(media_out_dir)
    core.ask(prompt, client.get("endpoint"),
             system=system_text if has_system else None,
             api_key=client.get("api_key"), model=client.get("model"),
             timeout_ms=client.get("timeout_ms"),
             temperature=client.get("temperature"), top_p=client.get("top_p"),
             presence_penalty=client.get("presence_penalty"),
             frequency_penalty=client.get("frequency_penalty"),
             max_tokens=client.get("max_tokens"), seed=client.get("seed"),
             stream=stream, schema=schema_body, tools=tool_defs or None,
             media=media_items or None, modalities=modality_items or None,
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
