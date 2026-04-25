"""Tests for HTTPClient.

These tests route HTTPClient at an in-process FastAPI app via httpx.ASGITransport.
No real network required.
"""

from fastapi.testclient import TestClient
import pytest

from ai_core.functions.calculate import CalculateFunction
from ai_core.service.api import create_app
from ai_core.service.client import HTTPClient
from ai_core.service.core import CoreService


@pytest.fixture
def http_client() -> HTTPClient:
    """An HTTPClient routed to an in-process FastAPI app via TestClient."""
    core = CoreService()
    core.register(CalculateFunction("up", lambda b: b.upper()))
    app = create_app(core)

    client = HTTPClient(base_url="http://testserver")
    # Replace the network-backed httpx.Client with TestClient (which has the same sync API)
    client._client = TestClient(app, base_url="http://testserver")  # type: ignore
    return client


class TestExecute:
    def test_round_trip(self, http_client: HTTPClient) -> None:
        out, ctx = http_client.execute("up", b"hello", context={})
        assert out == b"HELLO"

    def test_context_round_trips(self, http_client: HTTPClient) -> None:
        _, ctx = http_client.execute("up", b"x", context={"foo": "bar"})
        assert ctx == {"foo": "bar"}


class TestListFunctions:
    def test_list(self, http_client: HTTPClient) -> None:
        names = http_client.list_functions()
        assert "up" in names
