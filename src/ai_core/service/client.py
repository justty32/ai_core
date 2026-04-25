"""HTTPClient — Python-side wrapper for the Core HTTP API.

Handles base64 encoding so callers can pass raw bytes naturally:

    client = HTTPClient("http://localhost:8000")
    output_bytes, context = client.execute("my_func", b"hello", context={})
"""

from typing import Any

import httpx


class HTTPClient:
    """HTTP client for ai_core's REST API."""

    def __init__(
        self,
        base_url: str = "http://localhost:8000",
        timeout: float = 30.0,
    ) -> None:
        """Configure the client.

        Args:
            base_url: API root, no trailing slash
            timeout: per-request timeout in seconds
        """
        raise NotImplementedError

    def execute(
        self,
        func_id: str,
        tokens: bytes,
        context: dict[str, Any] | None = None,
    ) -> tuple[bytes, dict[str, Any]]:
        """Call POST /execute.

        Steps:
            1. Encode tokens to base64 (string).
            2. POST {func_id, tokens, context} to /execute.
            3. Raise for non-200 (use response.raise_for_status()).
            4. Decode response.tokens from base64 back to bytes.
            5. Return (decoded_tokens, response.context).
        """
        raise NotImplementedError

    def list_functions(self) -> list[str]:
        """Call GET /functions."""
        raise NotImplementedError

    def get_function_info(self, func_id: str) -> dict[str, Any]:
        """Call GET /functions/{func_id}. Returns the FunctionInfo dict."""
        raise NotImplementedError
