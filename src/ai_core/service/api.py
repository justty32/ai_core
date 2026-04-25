"""FastAPI app for the Core service.

Use the `create_app(core)` factory to build an app bound to a particular
CoreService instance. This pattern lets us spin up isolated apps for
testing without module-level state.

Endpoints:
    POST /execute              -> ExecuteResponse
    GET  /functions            -> list[str]
    GET  /functions/{func_id}  -> FunctionInfo

Errors:
    Functions raise FunctionNotFoundError -> 404 with ErrorResponse
    Other exceptions                      -> 500 with ErrorResponse
"""

from fastapi import FastAPI

from ai_core.service.core import CoreService
from ai_core.service.models import (
    ExecuteRequest,
    ExecuteResponse,
    FunctionInfo,
)


def create_app(core: CoreService) -> FastAPI:
    """Build a FastAPI app bound to the given CoreService.

    Steps to implement:
        1. Create FastAPI(title="ai_core", version="0.1.0").
        2. Define exception handlers for FunctionNotFoundError (404) and
           generic Exception (500), returning ErrorResponse-shaped JSON.
        3. Define the routes below.
        4. Return the app.

    The route handlers should:
        - decode req.tokens (base64 -> bytes)
        - call core.execute(...)
        - encode result tokens (bytes -> base64)
        - return ExecuteResponse(tokens=..., context=...)
    """
    raise NotImplementedError


# Below are skeleton route signatures for reference (the implementer should
# nest these inside `create_app` so they close over `core`):
#
# @app.post("/execute", response_model=ExecuteResponse)
# def execute(req: ExecuteRequest) -> ExecuteResponse: ...
#
# @app.get("/functions", response_model=list[str])
# def list_functions() -> list[str]: ...
#
# @app.get("/functions/{func_id}", response_model=FunctionInfo)
# def get_function(func_id: str) -> FunctionInfo: ...


__all__ = ["create_app", "ExecuteRequest", "ExecuteResponse", "FunctionInfo"]
