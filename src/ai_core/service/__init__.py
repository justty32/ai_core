"""Layer 3 — Client-server interface.

The Core is exposed via:
    - CoreService: the in-process Python interface
    - create_app(): a FastAPI app factory for HTTP API
    - HTTPClient: a Python wrapper for calling the HTTP API

Sessions are NOT managed here. Clients pass their own context dict on
every request, and the server returns the (possibly mutated) context
back. The core is stateless w.r.t. user sessions.
"""

from ai_core.service.api import create_app
from ai_core.service.client import HTTPClient
from ai_core.service.core import CoreService
from ai_core.service.models import (
    ErrorResponse,
    ExecuteRequest,
    ExecuteResponse,
    FunctionInfo,
)

__all__ = [
    "CoreService",
    "create_app",
    "HTTPClient",
    "ExecuteRequest",
    "ExecuteResponse",
    "FunctionInfo",
    "ErrorResponse",
]
