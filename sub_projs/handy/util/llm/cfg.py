"""cfg.py — CLI 的 config 檔那一層（沿用已封存 pllm 的 config.py）。

設定來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本檔只管中間
那層；旗標覆寫在 cli.py。路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
未列鍵靜默忽略（lenient）。

⚠ 與 `util.config`（handy 的 $env／$ref 設定讀取器）無關，那是另一回事。
"""
import json
import os
import sys

from .errors import CONFIG_ENV_VAR, EXIT_OK, EXIT_USAGE
from .flags import REFLECT_FLAGS

# config 檔允許的鍵＝反射旗標的欄位；未列鍵靜默忽略。
CONFIG_KEYS = set()
for _flag, _field, _typ in REFLECT_FLAGS:
    CONFIG_KEYS.add(_field)


def default_config_path():
    """~/.config/llm/config.json（沒有 HOME 就當沒有預設路徑）。"""
    home = os.environ.get("HOME")
    if not home:
        return None
    return os.path.join(home, ".config", "llm", "config.json")


def _pick_path(has_config, config_path):
    """決定要讀哪個 config 檔，回 (路徑, 是否被明指)。"""
    if has_config:
        return config_path, True
    if os.environ.get(CONFIG_ENV_VAR):
        return os.environ[CONFIG_ENV_VAR], True
    return default_config_path(), False


def load_config(client, has_config, config_path):
    """把 config 檔的值填進 client dict。回退出碼。"""
    path, named = _pick_path(has_config, config_path)
    if not path:
        return EXIT_OK

    try:
        with open(path, "rb") as f:
            body = f.read().decode("utf-8")
    except OSError:
        if named:      # 明指卻讀不到＝用法錯（點名是誰指的路）
            who = "--config" if has_config else CONFIG_ENV_VAR
            sys.stderr.write("讀不到檔案：%s（%s 指定的 config 檔）\n"
                             % (path, who))
            return EXIT_USAGE
        return EXIT_OK  # 探測路徑讀不到＝沒設定檔，靜默用預設

    try:
        data = json.loads(body)
    except Exception:  # noqa: BLE001
        sys.stderr.write("config JSON 解析失敗（%s）\n" % path)
        return EXIT_USAGE

    if isinstance(data, dict):
        for k, v in data.items():
            if k in CONFIG_KEYS:
                client[k] = v
    return EXIT_OK
