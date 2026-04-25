"""Tests for BaseFunction (abstract base)."""

import pytest

from ai_core.functions.base import BaseFunction, _default_metadata


class TestDefaultMetadata:
    def test_returns_dict_with_id(self) -> None:
        """_default_metadata fills the id field."""
        md = _default_metadata("foo_v1")
        assert md["id"] == "foo_v1"

    def test_returns_fresh_dict_each_call(self) -> None:
        """Two calls produce independent dicts (no shared refs)."""
        a = _default_metadata("a")
        b = _default_metadata("b")
        a["tags"].append("X")
        assert "X" not in b["tags"]

    def test_has_all_expected_top_level_keys(self) -> None:
        """The default metadata has all the canonical fields."""
        md = _default_metadata("x")
        for key in (
            "id", "type", "version", "expanded_name", "tags",
            "grouping", "conceptual_purpose", "domain",
            "resource_profile", "input_requirements", "output_produces",
        ):
            assert key in md


class TestBaseFunctionAbstract:
    def test_cannot_instantiate_directly(self) -> None:
        """BaseFunction is abstract — direct instantiation should fail."""
        with pytest.raises(TypeError):
            BaseFunction("x")  # type: ignore[abstract]
