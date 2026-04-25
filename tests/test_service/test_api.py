"""Tests for the FastAPI app."""

import base64

import pytest
from fastapi.testclient import TestClient

from ai_core.functions.calculate import CalculateFunction
from ai_core.service.api import create_app
from ai_core.service.core import CoreService


@pytest.fixture
def client() -> TestClient:
    """A TestClient bound to a CoreService with one upper-case function."""
    core = CoreService()
    core.register(CalculateFunction("up", lambda b: b.upper()))
    app = create_app(core)
    return TestClient(app)


def _b64(b: bytes) -> str:
    return base64.b64encode(b).decode()


def _decode(s: str) -> bytes:
    return base64.b64decode(s.encode())


class TestExecuteEndpoint:
    def test_happy_path(self, client: TestClient) -> None:
        resp = client.post(
            "/execute",
            json={"func_id": "up", "tokens": _b64(b"hello"), "context": {}},
        )
        assert resp.status_code == 200
        body = resp.json()
        assert _decode(body["tokens"]) == b"HELLO"
        assert body["context"] == {}

    def test_unknown_function_returns_404(self, client: TestClient) -> None:
        resp = client.post(
            "/execute",
            json={"func_id": "nope", "tokens": _b64(b"x"), "context": {}},
        )
        assert resp.status_code == 404
        body = resp.json()
        assert "error" in body

    def test_context_round_trips(self, client: TestClient) -> None:
        resp = client.post(
            "/execute",
            json={
                "func_id": "up",
                "tokens": _b64(b"x"),
                "context": {"foo": "bar"},
            },
        )
        assert resp.json()["context"] == {"foo": "bar"}


class TestFunctionsEndpoint:
    def test_list_functions(self, client: TestClient) -> None:
        resp = client.get("/functions")
        assert resp.status_code == 200
        assert "up" in resp.json()

    def test_get_function_info(self, client: TestClient) -> None:
        resp = client.get("/functions/up")
        assert resp.status_code == 200
        body = resp.json()
        assert body["id"] == "up"
        assert body["type"] == "calculate"

    def test_get_unknown_returns_404(self, client: TestClient) -> None:
        resp = client.get("/functions/nope")
        assert resp.status_code == 404
