"""Tests for FunctionRegistry."""

import pytest

from ai_core.functions.calculate import CalculateFunction
from ai_core.functions.composite import CompositeFunction
from ai_core.registry.exceptions import (
    FunctionAlreadyExistsError,
    FunctionInUseError,
    FunctionNotFoundError,
)
from ai_core.registry.registry import FunctionRegistry


class TestRegister:
    def test_register_stores_function(self) -> None:
        reg = FunctionRegistry()
        f = CalculateFunction("a", lambda b: b)
        reg.register(f)
        assert reg.has("a")

    def test_duplicate_raises(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        with pytest.raises(FunctionAlreadyExistsError):
            reg.register(CalculateFunction("a", lambda b: b))

    def test_indexes_by_type(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        results = reg.find_by_type("calculate")
        assert len(results) == 1
        assert results[0].id == "a"

    def test_indexes_by_tag(self) -> None:
        reg = FunctionRegistry()
        f = CalculateFunction("a", lambda b: b)
        f.metadata["tags"] = ["foo", "bar"]
        reg.register(f)
        assert len(reg.find_by_tag("foo")) == 1
        assert len(reg.find_by_tag("bar")) == 1
        assert len(reg.find_by_tag("baz")) == 0


class TestUnregister:
    def test_unregister_removes(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        reg.unregister("a")
        assert not reg.has("a")

    def test_unregister_unknown_raises(self) -> None:
        reg = FunctionRegistry()
        with pytest.raises(FunctionNotFoundError):
            reg.unregister("missing")

    def test_unregister_in_use_raises(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        reg.register(CompositeFunction("pipe", ["a"], reg))
        with pytest.raises(FunctionInUseError):
            reg.unregister("a")

    def test_can_unregister_after_referrer_removed(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        reg.register(CompositeFunction("pipe", ["a"], reg))
        reg.unregister("pipe")
        reg.unregister("a")  # should not raise
        assert not reg.has("a")


class TestLookup:
    def test_get_returns_function(self) -> None:
        reg = FunctionRegistry()
        f = CalculateFunction("a", lambda b: b)
        reg.register(f)
        assert reg.get("a") is f

    def test_get_unknown_raises(self) -> None:
        reg = FunctionRegistry()
        with pytest.raises(FunctionNotFoundError):
            reg.get("nope")

    def test_list_all_returns_sorted(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("c", lambda b: b))
        reg.register(CalculateFunction("a", lambda b: b))
        reg.register(CalculateFunction("b", lambda b: b))
        assert reg.list_all() == ["a", "b", "c"]


class TestReferences:
    def test_get_references_returns_referrers(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        reg.register(CompositeFunction("pipe1", ["a"], reg))
        reg.register(CompositeFunction("pipe2", ["a"], reg))
        assert reg.get_references("a") == {"pipe1", "pipe2"}

    def test_get_references_empty_for_unreferenced(self) -> None:
        reg = FunctionRegistry()
        reg.register(CalculateFunction("a", lambda b: b))
        assert reg.get_references("a") == set()

    def test_get_references_unknown_returns_empty(self) -> None:
        """Asking about an unregistered ID returns empty (does not raise)."""
        reg = FunctionRegistry()
        assert reg.get_references("nope") == set()
