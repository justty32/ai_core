"""config.py — 三層 config 來源解析（對齊 core/src/cli_config.cpp 的 load_into）。

來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本模組只處理「config 檔」這層
（前二層）；命令列旗標覆寫在 cli.py。config 檔路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
未列鍵靜默忽略（lenient）。感覺基準＝core-py 的 cllm/config.py。
"""
import json
import os
import sys

from flags import REFLECT_FLAGS
from internal import CONFIG_ENV_VAR, EXIT_OK, EXIT_USAGE, read_file_text

# config 檔允許的鍵（對齊 llm_client_t 欄位；未列鍵靜默忽略＝lenient）。
CONFIG_KEYS = {field for _, field, _ in REFLECT_FLAGS}


def default_config_path():
    """~/.config/llm/config.json（對齊 cli_config.cpp：靠 HOME）。"""
    home = os.environ.get("HOME")
    if not home:
        return None
    return os.path.join(home, ".config", "llm", "config.json")


def load_config(client, has_config, config_path):
    """三層 config 來源前二層（對齊 cli_config.cpp 的 load_into）。回退出碼。"""
    named = False
    if has_config:
        cfg_path, named = config_path, True
    elif os.environ.get(CONFIG_ENV_VAR):
        cfg_path, named = os.environ[CONFIG_ENV_VAR], True
    else:
        cfg_path = default_config_path()
    if not cfg_path:
        return EXIT_OK

    try:
        body = read_file_text(cfg_path)
    except OSError:
        if named:  # 明指卻讀不到＝用法錯（點名是誰指的路）
            who = "--config" if has_config else CONFIG_ENV_VAR
            sys.stderr.write("讀不到檔案：%s（%s 指定的 config 檔）\n" % (cfg_path, who))
            return EXIT_USAGE
        return EXIT_OK  # 探測路徑讀不到＝沒設定檔，靜默用預設
    try:
        data = json.loads(body)
    except Exception:
        sys.stderr.write("config JSON 解析失敗（%s）\n" % cfg_path)
        return EXIT_USAGE
    if isinstance(data, dict):
        for k, v in data.items():
            if k in CONFIG_KEYS:
                client[k] = v
    return EXIT_OK
