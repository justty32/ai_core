import json
import pytest
from pathlib import Path
from session_lib import Session


@pytest.fixture
def tmp_session(tmp_path):
    return Session(tmp_path / "session.json")


def test_get_default(tmp_session):
    assert tmp_session.get("missing") is None
    assert tmp_session.get("missing", "default") == "default"


def test_set_and_get(tmp_session):
    tmp_session.set("key", "value")
    assert tmp_session.get("key") == "value"


def test_persists_across_instances(tmp_path):
    s1 = Session(tmp_path / "session.json")
    s1.set("history", [{"role": "user", "content": "hi"}])

    s2 = Session(tmp_path / "session.json")
    assert s2.get("history") == [{"role": "user", "content": "hi"}]


def test_delete(tmp_session):
    tmp_session.set("x", 1)
    tmp_session.delete("x")
    assert tmp_session.get("x") is None


def test_clear(tmp_session):
    tmp_session.set("a", 1)
    tmp_session.set("b", 2)
    tmp_session.clear()
    assert tmp_session.all() == {}


def test_atomic_write_creates_no_tmp(tmp_path):
    s = Session(tmp_path / "session.json")
    s.set("k", "v")
    assert not (tmp_path / "session.tmp").exists()
    assert (tmp_path / "session.json").exists()


def test_all_returns_copy(tmp_session):
    tmp_session.set("x", 1)
    d = tmp_session.all()
    d["x"] = 999
    assert tmp_session.get("x") == 1
