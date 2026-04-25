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

import base64

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from ai_core.registry.exceptions import FunctionNotFoundError
from ai_core.service.core import CoreService
from ai_core.service.models import (
    ExecuteRequest,
    ExecuteResponse,
    FunctionInfo,
)


def create_app(core: CoreService) -> FastAPI:
    """Build a FastAPI app bound to the given CoreService."""
    app = FastAPI(title="ai_core", version="0.1.0")

    @app.exception_handler(FunctionNotFoundError)
    async def _not_found_handler(request: Request, exc: FunctionNotFoundError):
        return JSONResponse(
            status_code=404,
            content={"error": "function_not_found", "detail": str(exc)},
        )

    @app.post("/execute", response_model=ExecuteResponse)
    def execute_endpoint(req: ExecuteRequest) -> ExecuteResponse:
        tokens = base64.b64decode(req.tokens.encode())
        out_tokens, out_context = core.execute(req.func_id, tokens, req.context)
        return ExecuteResponse(
            tokens=base64.b64encode(out_tokens).decode(),
            context=out_context,
        )

    @app.get("/functions", response_model=list[str])
    def list_endpoint() -> list[str]:
        return core.list_functions()

    @app.get("/functions/{func_id}", response_model=FunctionInfo)
    def info_endpoint(func_id: str) -> FunctionInfo:
        info = core.get_function_info(func_id)
        return FunctionInfo(**info)

    return app


__all__ = ["create_app", "ExecuteRequest", "ExecuteResponse", "FunctionInfo"]
