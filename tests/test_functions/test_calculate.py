"""Tests for CalculateFunction."""

import pytest

from ai_core.functions.calculate import CalculateFunction


class TestConstruction:
    def test_stores_func(self) -> None:
        f = CalculateFunction("up_v1", lambda b: b.upper())
        assert f.id == "up_v1"
        assert "func" in f.closure

    def test_metadata_type_is_calculate(self) -> None:
        f = CalculateFunction("up_v1", lambda b: b)
        assert f.metadata["type"] == "calculate"


class TestCall:
    def test_applies_callable(self) -> None:
        f = CalculateFunction("up_v1", lambda b: b.upper())
        out, _ = f(b"hello", {})
        assert out == b"HELLO"

    def test_passes_context_through_unchanged(self) -> None:
        f = CalculateFunction("up_v1", lambda b: b)
        ctx = {"x": 1}
        _, out_ctx = f(b"x", ctx)
        assert out_ctx == {"x": 1}

    def test_str_result_is_encoded(self) -> None:
        """If the wrapped callable returns str, it should be encoded to bytes."""
        f = CalculateFunction("str_v1", lambda b: "result_str")
        out, _ = f(b"x", {})
        assert out == b"result_str"


class TestSerialization:
    def test_to_dict_raises(self) -> None:
        """Calculate functions can't be JSON-serialized."""
        f = CalculateFunction("up_v1", lambda b: b)
        with pytest.raises(NotImplementedError):
            f.to_dict()

    def test_from_dict_raises(self) -> None:
        with pytest.raises(NotImplementedError):
            CalculateFunction.from_dict({})
