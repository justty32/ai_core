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


def test_build_list_marks_failing_metadata_absent(tmp_path):
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
    by_name = {e["name"]: e for e in entries}
    assert by_name["good"]["_status"] == "ok"
    assert by_name["bad"]["_status"] == "absent"
    assert "_warning" in by_name["bad"]


def test_search_empty_query_errors(tmp_path):
    out = tmp_path / "list.json"
    out.write_text("[]")
    r = subprocess.run(
        [sys.executable, str(HUB), "--search-func", "", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 2
    assert "non-empty" in r.stderr


def test_build_list_marks_missing_required_field_partial(tmp_path):
    # metadata missing 'description'
    make_process(tmp_path, "noop", {"name": "noop", "tags": []})
    out = tmp_path / "list.json"
    r = subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True, text=True,
    )
    entries = json.loads(out.read_text())
    assert len(entries) == 1
    assert entries[0]["_status"] == "partial"
    assert "missing required fields" in entries[0]["_warning"]
    assert "missing required fields" in r.stderr


def test_build_list_reports_degraded(tmp_path):
    make_process(tmp_path, "good", {"name": "good", "description": "ok"})
    make_bad_process(tmp_path, "broken", 'import sys; sys.argv  # no --metadata handling\n')
    out = tmp_path / "list.json"
    r = subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True, text=True,
    )
    assert "degraded" in r.stdout
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
    assert "MetadataAbsent" in r.stderr


def test_json_errors_format(tmp_path):
    p = tmp_path / "noexec.py"
    p.write_text("#!/usr/bin/env python3\nprint('hi')\n")
    p.chmod(0o644)
    r = subprocess.run(
        [sys.executable, str(HUB), "--json-errors", "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 2
    line = r.stderr.strip().splitlines()[0]
    obj = json.loads(line)
    assert obj["type"] == "NotExecutable"
    assert "message" in obj
    assert "retriable" in obj


def test_validate_runs_examples(tmp_path):
    p = tmp_path / "upper.py"
    p.write_text("""#!/usr/bin/env python3
import sys, json
META = {
    "name": "upper",
    "description": "uppercase",
    "examples": [{"input": "hi", "output": "HI"}],
}
if "--metadata" in sys.argv:
    print(json.dumps(META))
else:
    print(sys.stdin.read().upper(), end="")
""")
    p.chmod(0o755)
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    assert "1 example(s) passed" in r.stdout


def test_gen_agents_md(tmp_path):
    make_process(tmp_path, "foo", {"name": "foo", "description": "does foo", "tags": ["a"]})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True,
    )
    r = subprocess.run(
        [sys.executable, str(HUB), "--gen-agents-md", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    assert "Agent Guide" in r.stdout
    assert "foo" in r.stdout


def test_gen_functions_md(tmp_path):
    make_process(tmp_path, "foo", {"name": "foo", "description": "does foo", "tags": ["a"]})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True,
    )
    r = subprocess.run(
        [sys.executable, str(HUB), "--gen-functions-md", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    assert "## `foo`" in r.stdout
    assert "does foo" in r.stdout


def test_export_openai_tools(tmp_path):
    make_process(tmp_path, "foo", {"name": "foo", "description": "does foo", "input": "text"})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True,
    )
    r = subprocess.run(
        [sys.executable, str(HUB), "--export", "openai-tools", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    tools = json.loads(r.stdout)
    assert tools[0]["type"] == "function"
    assert tools[0]["function"]["name"] == "foo"
    assert "input" in tools[0]["function"]["parameters"]["properties"]


def test_export_anthropic_tools(tmp_path):
    make_process(tmp_path, "foo", {"name": "foo", "description": "does foo"})
    out = tmp_path / "list.json"
    subprocess.run(
        [sys.executable, str(HUB), "--build-list", str(tmp_path), "--output", str(out)],
        capture_output=True,
    )
    r = subprocess.run(
        [sys.executable, str(HUB), "--export", "anthropic-tools", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    tools = json.loads(r.stdout)
    assert tools[0]["name"] == "foo"
    assert "input_schema" in tools[0]


def test_validate_warns_on_bad_io(tmp_path):
    p = tmp_path / "bad_io.py"
    p.write_text("""#!/usr/bin/env python3
import sys, json
META = {
    "name": "bad_io",
    "description": "wrong io values",
    "io": {"input": "bogus", "output": "stdout"},
}
if "--metadata" in sys.argv:
    print(json.dumps(META))
else:
    print(sys.stdin.read(), end="")
""")
    p.chmod(0o755)
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0  # warn-only
    assert "InvalidIo" in r.stderr
    assert "bogus" in r.stderr


def test_validate_accepts_valid_io(tmp_path):
    p = tmp_path / "ok_io.py"
    p.write_text("""#!/usr/bin/env python3
import sys, json
META = {
    "name": "ok_io",
    "description": "good io",
    "io": {"input": "stdin", "output": "stdout", "format": {"input": "text", "output": "json"}},
}
if "--metadata" in sys.argv:
    print(json.dumps(META))
else:
    print(sys.stdin.read(), end="")
""")
    p.chmod(0o755)
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0
    assert "InvalidIo" not in r.stderr


def test_export_unknown_format(tmp_path):
    out = tmp_path / "list.json"
    out.write_text("[]")
    r = subprocess.run(
        [sys.executable, str(HUB), "--export", "bogus", "--list", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 2
    assert "UnknownFormat" in r.stderr


def test_validate_fails_on_example_mismatch(tmp_path):
    p = tmp_path / "wrong.py"
    p.write_text("""#!/usr/bin/env python3
import sys, json
META = {
    "name": "wrong",
    "description": "wrong",
    "examples": [{"input": "hi", "output": "WRONG_EXPECTED"}],
}
if "--metadata" in sys.argv:
    print(json.dumps(META))
else:
    print(sys.stdin.read().upper(), end="")
""")
    p.chmod(0o755)
    r = subprocess.run(
        [sys.executable, str(HUB), "--validate", str(p)],
        capture_output=True, text=True,
    )
    assert r.returncode == 1
    assert "ExampleFailed" in r.stderr
    assert "output mismatch" in r.stderr
