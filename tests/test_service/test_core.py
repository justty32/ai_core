"""Tests for CoreService."""

import pytest

from ai_core.functions.calculate import CalculateFunction
from ai_core.registry.exceptions import FunctionNotFoundError
from ai_core.service.core import CoreService


class TestRegister:
    def test_register_then_list(self) -> None:
        core = CoreService()
        core.register(CalculateFunction("a", lambda b: b))
        assert "a" in core.list_functions()

    def test_unregister(self) -> None:
        core = CoreService()
        core.register(CalculateFunction("a", lambda b: b))
        core.unregister("a")
        assert "a" not in core.list_functions()


class TestExecute:
    def test_execute_returns_result(self) -> None:
        core = CoreService()
        core.register(CalculateFunction("up", lambda b: b.upper()))
        out, _ = core.execute("up", b"hi", {})
        assert out == b"HI"

    def test_execute_unknown_raises(self) -> None:
        core = CoreService()
        with pytest.raises(FunctionNotFoundError):
            core.execute("nope", b"x", {})

    def test_context_defaults_to_empty_dict(self) -> None:
        """Calling without context shouldn't crash; should default to {}."""
        core = CoreService()
        core.register(CalculateFunction("up", lambda b: b.upper()))
        out, ctx = core.execute("up", b"hi")
        assert out == b"HI"
        assert isinstance(ctx, dict)


class TestFunctionInfo:
    def test_returns_metadata(self) -> None:
        core = CoreService()
        f = CalculateFunction("a", lambda b: b)
        f.metadata["tags"] = ["test"]
        core.register(f)

        info = core.get_function_info("a")
        assert info["id"] == "a"
        assert info["type"] == "calculate"
        assert "tags" in info["metadata"]

    def test_unknown_raises(self) -> None:
        core = CoreService()
        with pytest.raises(FunctionNotFoundError):
            core.get_function_info("nope")
