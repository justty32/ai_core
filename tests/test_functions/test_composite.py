"""Tests for CompositeFunction."""

import pytest

from ai_core.functions.calculate import CalculateFunction
from ai_core.functions.composite import CompositeFunction
from ai_core.registry.exceptions import FunctionNotFoundError
from ai_core.registry.registry import FunctionRegistry


class TestConstruction:
    def test_stores_chain_and_registry(self) -> None:
        reg = FunctionRegistry()
        f = CompositeFunction("pipe_v1", ["a", "b"], reg)
        assert f.id == "pipe_v1"
        assert f.closure["func_chain"] == ["a", "b"]
        assert f.closure["registry"] is reg

    def test_metadata_type_is_composite(self) -> None:
        reg = FunctionRegistry()
        f = CompositeFunction("pipe_v1", [], reg)
        assert f.metadata["type"] == "composite"


class TestCall:
    def test_chains_outputs_in_order(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b + b"-a"))
        reg.register(CalculateFunction("b", lambda b: b + b"-b"))

        pipe = CompositeFunction("pipe_v1", ["a", "b"], reg)
        out, _ = pipe(b"x", {})
        assert out == b"x-a-b"

    def test_threads_context(self) -> None:
        """Each function's context mutations should be visible to the next."""
        reg = FunctionRegistry()

        def f1(tokens: bytes, ctx: dict) -> tuple[bytes, dict]:
            ctx["step1"] = True
            return tokens, ctx

        def f2(tokens: bytes, ctx: dict) -> tuple[bytes, dict]:
            ctx["step2"] = ctx.get("step1", False)
            return tokens, ctx

        # Use CalculateFunction-like classes via direct register; simulate behavior
        # via a tiny inline subclass for this test.
        class _Fn:
            def __init__(self, fid: str, fn) -> None:
                self.id = fid
                self.closure = {}
                self.metadata = {"type": "x", "tags": [], "id": fid}
                self._fn = fn

            def __call__(self, tokens, ctx):
                return self._fn(tokens, ctx)

        reg._functions["f1"] = _Fn("f1", f1)  # type: ignore[assignment]
        reg._functions["f2"] = _Fn("f2", f2)  # type: ignore[assignment]

        pipe = CompositeFunction("pipe_v1", ["f1", "f2"], reg)
        _, ctx = pipe(b"x", {})
        assert ctx["step1"] is True
        assert ctx["step2"] is True

    def test_missing_function_raises(self) -> None:
        reg = FunctionRegistry()
        pipe = CompositeFunction("pipe_v1", ["nonexistent"], reg)
        with pytest.raises(FunctionNotFoundError):
            pipe(b"x", {})


class TestSerialization:
    def test_to_dict_excludes_registry(self) -> None:
        """Registry is not JSON-serializable; only func_chain should appear."""
        reg = FunctionRegistry()
        f = CompositeFunction("pipe_v1", ["a", "b"], reg)
        d = f.to_dict()
        assert d["closure"]["func_chain"] == ["a", "b"]
        assert "registry" not in d["closure"]

    def test_from_dict_rebinds_registry(self) -> None:
        reg = FunctionRegistry()
        data = {
            "type": "composite",
            "id": "pipe_v1",
            "closure": {"func_chain": ["a", "b"]},
            "metadata": {"id": "pipe_v1", "type": "composite", "tags": []},
        }
        f = CompositeFunction.from_dict(data, registry=reg)
        assert f.closure["registry"] is reg
        assert f.closure["func_chain"] == ["a", "b"]
