"""Tests for ShellFunction.

Uses real subprocess invocations against a small bash script created in tmp_path.
"""

import stat
from pathlib import Path

import pytest

from ai_core.functions.shell import ShellFunction


@pytest.fixture
def echo_script(tmp_path: Path) -> str:
    """Create a script that echoes its first arg."""
    p = tmp_path / "echo.sh"
    p.write_text("#!/usr/bin/env bash\necho -n \"$1\"\n")
    p.chmod(p.stat().st_mode | stat.S_IEXEC)
    return str(p)


@pytest.fixture
def fail_script(tmp_path: Path) -> str:
    """Create a script that exits non-zero."""
    p = tmp_path / "fail.sh"
    p.write_text("#!/usr/bin/env bash\necho 'oops' >&2\nexit 1\n")
    p.chmod(p.stat().st_mode | stat.S_IEXEC)
    return str(p)


@pytest.fixture
def slow_script(tmp_path: Path) -> str:
    """Create a script that sleeps longer than the test timeout."""
    p = tmp_path / "slow.sh"
    p.write_text("#!/usr/bin/env bash\nsleep 5\n")
    p.chmod(p.stat().st_mode | stat.S_IEXEC)
    return str(p)


class TestConstruction:
    def test_stores_closure(self, echo_script: str) -> None:
        f = ShellFunction("echo_v1", echo_script, timeout=10)
        assert f.id == "echo_v1"
        assert f.closure["script_path"] == echo_script
        assert f.closure["timeout"] == 10

    def test_metadata_type_is_shell(self, echo_script: str) -> None:
        f = ShellFunction("echo_v1", echo_script)
        assert f.metadata["type"] == "shell"


class TestCall:
    def test_returns_stdout(self, echo_script: str) -> None:
        f = ShellFunction("echo_v1", echo_script)
        out, _ = f(b"hello", {})
        assert out == b"hello"

    def test_does_not_modify_context(self, echo_script: str) -> None:
        f = ShellFunction("echo_v1", echo_script)
        ctx = {"key": "value"}
        _, out_ctx = f(b"x", ctx)
        assert out_ctx == {"key": "value"}

    def test_non_zero_exit_raises(self, fail_script: str) -> None:
        f = ShellFunction("fail_v1", fail_script)
        with pytest.raises(RuntimeError):
            f(b"input", {})

    def test_timeout_raises(self, slow_script: str) -> None:
        import subprocess
        f = ShellFunction("slow_v1", slow_script, timeout=1)
        with pytest.raises(subprocess.TimeoutExpired):
            f(b"input", {})


class TestSerialization:
    def test_round_trip(self, echo_script: str) -> None:
        f = ShellFunction("echo_v1", echo_script, timeout=15)
        restored = ShellFunction.from_dict(f.to_dict())
        assert restored.closure == f.closure
        assert restored.id == f.id
