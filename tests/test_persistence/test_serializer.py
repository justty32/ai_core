"""Tests for save/load of CoreService."""

import json
from pathlib import Path

import pytest

from ai_core.functions.calculate import CalculateFunction
from ai_core.functions.composite import CompositeFunction
from ai_core.functions.llm import LLMFunction
from ai_core.functions.shell import ShellFunction
from ai_core.persistence.serializer import (
    SAVE_FORMAT_VERSION,
    load_core,
    save_core,
)
from ai_core.service.core import CoreService


class TestSave:
    def test_writes_envelope(self, tmp_path: Path) -> None:
        core = CoreService()
        core.register(LLMFunction("chat", model="claude-sonnet-4-6"))
        path = str(tmp_path / "core.json")

        save_core(core, path)

        data = json.loads(Path(path).read_text())
        assert data["version"] == SAVE_FORMAT_VERSION
        assert isinstance(data["functions"], list)
        assert len(data["functions"]) == 1
        assert data["functions"][0]["type"] == "llm"

    def test_calculate_skipped_with_warning(
        self, tmp_path: Path, caplog: pytest.LogCaptureFixture
    ) -> None:
        core = CoreService()
        core.register(LLMFunction("chat", model="claude-sonnet-4-6"))
        core.register(CalculateFunction("calc", lambda b: b))
        path = str(tmp_path / "core.json")

        save_core(core, path)

        data = json.loads(Path(path).read_text())
        types = {f["type"] for f in data["functions"]}
        assert "llm" in types
        assert "calculate" not in types
        # A warning should be logged about skipping the calculate function.
        assert any("calc" in r.message for r in caplog.records)


class TestLoad:
    def test_round_trip_llm_and_shell(self, tmp_path: Path) -> None:
        path = str(tmp_path / "core.json")
        # Make a tiny shell script to point at
        script = tmp_path / "echo.sh"
        script.write_text("#!/usr/bin/env bash\necho -n \"$1\"\n")
        script.chmod(0o755)

        core = CoreService()
        core.register(LLMFunction("chat", model="claude-sonnet-4-6"))
        core.register(ShellFunction("echo", str(script)))

        save_core(core, path)
        restored = load_core(path)

        assert "chat" in restored.list_functions()
        assert "echo" in restored.list_functions()

    def test_round_trip_composite_rebinds_registry(self, tmp_path: Path) -> None:
        path = str(tmp_path / "core.json")

        core = CoreService()
        core.register(LLMFunction("chat", model="claude-sonnet-4-6"))
        # Composite needs the registry passed at construction
        core.register(CompositeFunction("pipe", ["chat"], core.registry))

        save_core(core, path)
        restored = load_core(path)

        composite = restored.registry.get("pipe")
        # The composite's registry ref should be the *new* core's registry.
        assert composite.closure["registry"] is restored.registry

    def test_version_mismatch_raises(self, tmp_path: Path) -> None:
        path = tmp_path / "core.json"
        path.write_text(json.dumps({"version": "0.0.1", "functions": []}))
        with pytest.raises(ValueError):
            load_core(str(path))
