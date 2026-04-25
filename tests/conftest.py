"""Shared pytest fixtures for ai_core tests."""

from collections.abc import Iterator
from unittest.mock import MagicMock

import pytest

from ai_core.functions.calculate import CalculateFunction
from ai_core.registry.registry import FunctionRegistry
from ai_core.service.core import CoreService


@pytest.fixture
def empty_registry() -> FunctionRegistry:
    """A fresh, empty FunctionRegistry."""
    return FunctionRegistry()


@pytest.fixture
def empty_core() -> CoreService:
    """A fresh CoreService with empty registry."""
    return CoreService()


@pytest.fixture
def echo_calc() -> CalculateFunction:
    """A CalculateFunction that returns its input unchanged.

    Useful for unit-testing the registry, composites, and the service
    without needing real LLM calls.
    """
    return CalculateFunction("echo_v1", lambda tokens: tokens)


@pytest.fixture
def upper_calc() -> CalculateFunction:
    """A CalculateFunction that uppercases bytes."""
    return CalculateFunction("upper_v1", lambda tokens: tokens.upper())


@pytest.fixture
def mock_llm_client(monkeypatch: pytest.MonkeyPatch) -> Iterator[MagicMock]:
    """Replace the global llm_client with a MagicMock for the duration of the test.

    Tests can configure the mock's `.call.return_value` to script LLM responses.
    """
    from ai_core import llm_client as llm_module

    mock = MagicMock()
    mock.call.return_value = "mocked response"
    monkeypatch.setattr(llm_module, "llm_client", mock)
    yield mock
