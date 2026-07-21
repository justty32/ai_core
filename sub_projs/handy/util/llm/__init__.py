"""util.llm — handy 的 LLM 地基門面（litellm 包裝，形狀鏡像已封存的 pllm）。

住戶拿 LLM 當「腦」就吃這裡，介面與舊 `util.pllm` 逐項相同：

    from util.llm import ask          # 函式庫用法
    from util.llm import cli_main     # 借用整套 CLI（llme 就是這樣透傳）

換腦不換介面：內臟從自寫的 pllm 換成 litellm，`ask()` 簽章與 CLI 旗標照舊。
分歧與陷阱見 core.py 的模組 docstring 與 handy/README.md。
"""
from .cli import main as cli_main
from .core import DEFAULT_ENDPOINT, LLMError, ask

__all__ = ["ask", "cli_main", "LLMError", "DEFAULT_ENDPOINT"]
