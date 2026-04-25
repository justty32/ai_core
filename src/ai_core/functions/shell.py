"""ShellFunction — runs an external shell script via subprocess.

Closure:
    - script_path: absolute path to an executable script
    - timeout: max runtime in seconds

Behavior:
    - Pipes `tokens.decode()` as the first positional argument.
    - Captures stdout as the result.
    - Does NOT modify context.
    - Raises subprocess.TimeoutExpired if the script exceeds `timeout`.
"""

from typing import Any

from ai_core.functions.base import BaseFunction
from ai_core.types import Context, FunctionResult, Tokens


class ShellFunction(BaseFunction):
    """Wraps a shell script as a function."""

    def __init__(
        self,
        func_id: str,
        script_path: str,
        timeout: int = 30,
    ) -> None:
        """Construct a shell function.

        Args:
            func_id: unique identifier
            script_path: absolute path to the script (e.g. "/scripts/echo.sh")
            timeout: max runtime in seconds
        """
        raise NotImplementedError

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult:
        """Run the script with tokens as the first arg.

        Steps:
            1. Decode tokens to a string.
            2. Run subprocess: ["bash", script_path, decoded_input]
               with capture_output=True and timeout=self.closure["timeout"].
            3. Return (result.stdout, context).
            4. If non-zero exit code, raise RuntimeError with stderr.
        """
        raise NotImplementedError

    def to_dict(self) -> dict[str, Any]:
        """Serialize. Closure is JSON-safe."""
        raise NotImplementedError

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ShellFunction":
        """Reconstruct from a dict produced by to_dict()."""
        raise NotImplementedError
