import json
import subprocess
import sys
import pytest
from pathlib import Path

HUB = Path(__file__).parent.parent / "hub"


def make_process(tmp_path, name, metadata, will_execute=True):
    meta_json = json.dumps(metadata)
    content = f"""#!/usr/bin/env python3
import sys, json
if "--metadata" in sys.argv:
    print('{meta_json}')
else:
    print("ran")
"""
    p = tmp_path / f"{name}.py"
    p.write_text(content)
    p.chmod(0o755)
    return p


def make_bad_process(tmp_path, name, body):
    p = tmp_path / f"{name}.py"
    p.write_text("#!/usr/bin/env python3\n" + body)
    p.chmod(0o755)
    return p


def test_build_list(tmp_path):
    make_process(tmp_path, "foo", {"name": "foo", "description": "does foo", "tags": ["a"]})
    make_process(tmp_path, "bar", {"name": "bar", "description": "does bar", "tags": ["b"]})
    out = tmp_path / "list.json"

    r = subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True, text=True
    )
    assert r.returncode == 0
    entries = json.loads(out.read_text())
    names = [e["name"] for e in entries]
    assert "foo" in names
    assert "bar" in names


def test_build_list_includes_path(tmp_path):
    make_process(tmp_path, "baz", {"name": "baz", "description": "baz", "tags": []})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True, text=True
    )
    entries = json.loads(out.read_text())
    assert "path" in entries[0]


def test_search_func_by_name(tmp_path):
    make_process(tmp_path, "translate", {"name": "translate", "description": "translates text", "tags": []})
    make_process(tmp_path, "summarize", {"name": "summarize", "description": "summarizes text", "tags": []})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True
    )

    r = subprocess.run(
        [sys.executable, str(HUB), "--search-func", "translate", "--list", str(out)],
        capture_output=True, text=True
    )
    results = json.loads(r.stdout)
    assert len(results) == 1
    assert results[0]["name"] == "translate"


def test_search_func_by_tag(tmp_path):
    make_process(tmp_path, "denoise", {"name": "denoise", "description": "removes noise", "tags": ["audio", "preprocessing"]})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True
    )

    r = subprocess.run(
        [sys.executable, str(HUB), "--search-func", "audio", "--list", str(out)],
        capture_output=True, text=True
    )
    results = json.loads(r.stdout)
    assert any(e["name"] == "denoise" for e in results)


def test_search_func_no_match(tmp_path):
    out = tmp_path / "list.json"
    out.write_text("[]")

    r = subprocess.run(
        [sys.executable, str(HUB), "--search-func", "xyz", "--list", str(out)],
        capture_output=True, text=True
    )
    assert json.loads(r.stdout) == []


def test_build_list_skips_non_executable(tmp_path):
    make_process(tmp_path, "good", {"name": "good", "description": "ok", "tags": []})
    bad = tmp_path / "bad.py"
    bad.write_text("#!/usr/bin/env python3\nraise SystemExit(1)\n")
    bad.chmod(0o755)
    out = tmp_path / "list.json"

    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True
    )
    entries = json.loads(out.read_text())
    names = [e["name"] for e in entries]
    assert "good" in names
    assert "bad" not in names


def test_search_empty_query_errors(tmp_path):
    out = tmp_path / "list.json"
    out.write_text("[]")
    r = subprocess.run(
        [sys.executable, str(HUB), "--search-func", "", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 2
    assert "non-empty" in r.stderr


def test_build_list_skips_missing_required_field(tmp_path):
    # metadata missing 'description'
    make_process(tmp_path, "noop", {"name": "noop", "tags": []})
    out = tmp_path / "list.json"
    r = subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True, text=True,
    )
    entries = json.loads(out.read_text())
    assert entries == []
    assert "missing required fields" in r.stderr


def test_build_list_reports_failures(tmp_path):
    make_process(tmp_path, "good", {"name": "good", "description": "ok"})
    make_bad_process(tmp_path, "broken", 'import sys; sys.argv  # no --metadata handling\n')
    out = tmp_path / "list.json"
    r = subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True, text=True,
    )
    assert "Skipped" in r.stderr
    assert "broken" in r.stderr


def test_validate_ok(tmp_path):
    p = make_process(tmp_path, "ok", {"name": "ok", "description": "good"})
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    assert "OK:" in r.stdout


def test_validate_rejects_non_executable(tmp_path):
    p = tmp_path / "noexec.py"
    p.write_text("#!/usr/bin/env python3\nprint('hi')\n")
    p.chmod(0o644)
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 2
    assert "not executable" in r.stderr


def test_validate_rejects_missing_metadata(tmp_path):
    p = make_bad_process(tmp_path, "noscale", 'print("hi")\n')
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode != 0
    assert "FAIL" in r.stderr
