"""errors.py — 內核共用的錯誤型別與內建預設 endpoint（core 側的葉模組）。

葉模組：只依標準庫、不 import 套件內其他模組，被 request/http/response/stream/core 共用，
把內核側的依賴收成一張無環 DAG（對映 CLI 側的 internal.py）。LLMError 對應既有 ctypes
binding 的 LLMError；DEFAULT_ENDPOINT 的預設值對齊 core/src/cabi.cpp 的 make_request。
"""

# 內建預設 endpoint（對齊 core/src/cabi.cpp 的 make_request）。
DEFAULT_ENDPOINT = "http://localhost:1234/v1/chat/completions"


class LLMError(Exception):
    """傳輸／後端錯誤（未提供 on_error 時拋出；對應 binding 的 LLMError）。"""
