"""pllm — cllm 的純 Python 版（handy 的 LLM 地基；不碰 C ABI、直接打 OpenAI 相容 HTTP）。

內核在 core.py（可 import 直接用）；薄 CLI 外殼在 cli.py（`python -m pllm`／`pllm` 指令，
旗標與用法鏡像 cllm 的 `llm` CLI）。用法：

    from pllm import ask
    print(ask("你好", "file:///abs/path/to/fixture"))   # 離線自測用 file://

詳見 README.md 與 core.py 的模組 docstring。
"""
from .core import DEFAULT_ENDPOINT, LLMError, ask

__all__ = ["ask", "LLMError", "DEFAULT_ENDPOINT"]
__version__ = "0.1.0"
