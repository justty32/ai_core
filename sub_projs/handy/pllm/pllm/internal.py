"""internal.py — CLI 共用的退出碼、環境變數鍵、檔案讀取小工具（對齊 core/src/cli_internal.hpp）。

葉模組：只依標準庫、不 import 套件內其他模組，作為其餘 CLI 模組（flags/config/media/output/cli）
的共用底，把依賴收成一張無環 DAG。
"""

# 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
EXIT_OK = 0
EXIT_USAGE = 1
EXIT_REQUEST = 2
EXIT_CANCEL = 130

CONFIG_ENV_VAR = "LLM_CLI_CONFIG"  # 對齊 cli_internal.hpp 的 kConfigEnvVar


def read_file_bytes(path):
    """整檔讀成 raw bytes（媒體圖檔等）。讀不到拋 OSError，由呼叫端轉退出碼。"""
    with open(path, "rb") as f:
        return f.read()


def read_file_text(path):
    """整檔讀成 UTF-8 文字（config／media 描述子等）。讀不到拋 OSError。"""
    with open(path, "rb") as f:
        return f.read().decode("utf-8")
