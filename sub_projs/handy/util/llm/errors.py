"""errors.py — 共用葉：錯誤型別、內建預設端點、CLI 退出碼。

葉模組：不 import 套件內其他模組，讓依賴收成一張無環 DAG。
"""

DEFAULT_ENDPOINT = "http://localhost:1234/v1/chat/completions"

# 退出碼（沿用已封存 pllm）：0 成功；1 用法錯；2 請求失敗；130 取消。
EXIT_OK = 0
EXIT_USAGE = 1
EXIT_REQUEST = 2
EXIT_CANCEL = 130

CONFIG_ENV_VAR = "LLM_CLI_CONFIG"   # 只用來指定 config 檔路徑，不存設定值


class LLMError(Exception):
    """LLM 呼叫失敗（傳輸／後端／參數）。

    與已封存的 pllm 同名同位置，住戶原本的 except 不用改。
    """
