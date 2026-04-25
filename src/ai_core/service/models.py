"""pydantic request/response models for the HTTP API.

`tokens` is base64-encoded so binary content survives JSON transport.
"""

from typing import Any

from pydantic import BaseModel, Field


class ExecuteRequest(BaseModel):
    """Request body for POST /execute."""

    func_id: str = Field(description="Function ID to invoke")
    tokens: str = Field(description="Input bytes, base64-encoded")
    context: dict[str, Any] = Field(
        default_factory=dict,
        description="Session context (client-managed)",
    )


class ExecuteResponse(BaseModel):
    """Response body for POST /execute."""

    tokens: str = Field(description="Output bytes, base64-encoded")
    context: dict[str, Any] = Field(description="Updated context to round-trip")


class FunctionInfo(BaseModel):
    """Response body for GET /functions/{id}."""

    id: str
    type: str
    metadata: dict[str, Any]


class ErrorResponse(BaseModel):
    """Standard error envelope returned for non-200 responses."""

    error: str = Field(description="Short error code (e.g. 'function_not_found')")
    detail: str | None = Field(default=None, description="Human-readable details")
