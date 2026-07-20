"""env.py — handy 共用的環境變數小工具。

純讀 os.environ、無副作用；任何住戶要「從環境解金鑰/設定」時共用。
"""
import os


def first(*keys):
    """回第一個「有設且非空」的環境變數值；都沒有回 None。"""
    for k in keys:
        v = os.environ.get(k)
        if v:
            return v
    return None


def stem(name):
    """名字→env 慣例識別子：大寫、非英數轉 _（deep-seek → DEEP_SEEK）。"""
    return "".join(c if c.isalnum() else "_" for c in name.upper())
