"""pllm.py — util 便捷橋：定位並載入 handy 的 pllm 地基，轉出常用入口。

任何拿 LLM 當「腦」的住戶 `from util.pllm import ask`（或 cli_main）即可用，
不必各自算 pllm 路徑。pllm 套件在 handy 根的 pllm/ 下（本檔在 handy/util/）。
"""
import os
import sys

# handy 根＝本檔（handy/util/pllm.py）往上兩層；pllm 套件在 <根>/pllm/ 下。
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(_ROOT, "pllm"))

from pllm import LLMError, ask          # noqa: E402
from pllm.cli import main as cli_main   # noqa: E402

__all__ = ["ask", "cli_main", "LLMError"]
