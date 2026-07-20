"""smoke_harness.py — cli_smoke 的共用底：路徑、fixture URL、subprocess 跑 CLI、斷言與計數。

被 cli_smoke.py 與各 smoke_cases_* 模組共用，好把測試依分組拆檔而**計數狀態集中一處**：
expect_* 一律經 _ok/_ng 累加本模組的 _pass/_fail；跑完由 report() 一次讀出結果。走 file:// 假回應
（複用 core/test/fixtures），不連網。TEMP_FILES 收各 case 產生的暫存檔，跑完由 cleanup_temps() 清。
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)  # cllm-py/（cllm package 所在）
# fixtures 隨本副本自帶（test/fixtures/），故 handy 版自包含、離線可測；
# 無「../core」外部相依（源出 cllm/core/test/fixtures，見 README「來源」）。
FX_ROOT = os.path.join(HERE, "fixtures")

TEMP_FILES = []  # 各 case 產生的暫存檔；跑完統一清（保留「跑中建、跑完清」語意）

_pass = 0
_fail = 0


def fx(name):
    """組指向某 fixture 的 file:// endpoint（跨平台）。"""
    p = os.path.join(FX_ROOT, name, "chat", "completions").replace("\\", "/")
    return "file://" + p  # POSIX→file:///abs、Windows→file://C:/abs（內核都解得動）


def run(cli_args, stdin=None, extra_env=None):
    """跑 `python -m cllm <cli_args>`；回 (exit_code, stdout, stderr)。"""
    env = dict(os.environ)
    env["PYTHONPATH"] = PROJ + os.pathsep + env.get("PYTHONPATH", "")
    env["PYTHONIOENCODING"] = "utf-8"  # Windows 主控台預設非 UTF-8，強制對齊
    if extra_env:
        env.update(extra_env)
    p = subprocess.run([sys.executable, "-m", "cllm"] + cli_args,
                       input=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       env=env, encoding="utf-8")
    return p.returncode, p.stdout, p.stderr


def expect_out(needle, desc, cli_args, stdin=None, extra_env=None):
    code, out, err = run(cli_args, stdin=stdin, extra_env=extra_env)
    if code == 0 and needle in out:
        _ok(desc)
    else:
        _ng("%s（exit=%s out=<%s> err=<%s>）" % (desc, code, out.strip(), err.strip()))


def expect_body(needle, desc, cli_args, stdin=None):
    """LLM_DUMP_BODY=1 跑，驗印到 stderr 的請求 JSON 含子字串且退出碼 0。"""
    code, out, err = run(cli_args, stdin=stdin, extra_env={"LLM_DUMP_BODY": "1"})
    if code == 0 and needle in err:
        _ok(desc)
    else:
        _ng("%s（exit=%s body=<%s>）" % (desc, code, err.strip()))


def expect_no_body(needle, desc, cli_args):
    code, out, err = run(cli_args, extra_env={"LLM_DUMP_BODY": "1"})
    if code == 0 and needle not in err:
        _ok(desc)
    else:
        _ng("%s（exit=%s body=<%s>）" % (desc, code, err.strip()))


def expect_exit(want, desc, cli_args, stdin=None, extra_env=None):
    code, out, err = run(cli_args, stdin=stdin, extra_env=extra_env)
    if code == want:
        _ok("exit=%s  %s" % (code, desc))
    else:
        _ng("exit=%s（期望 %s）  %s（err=<%s>）" % (code, want, desc, err.strip()))


def _ok(desc):
    global _pass
    _pass += 1
    print("  [PASS] %s" % desc)


def _ng(desc):
    global _fail
    _fail += 1
    print("  [FAIL] %s" % desc, file=sys.stderr)


def cleanup_temps():
    """清掉各 case 登記的暫存檔（讀不到就算了）。"""
    for p in TEMP_FILES:
        try:
            os.remove(p)
        except OSError:
            pass


def report():
    """印總結，回退出碼（全過 0、任一敗 1）。"""
    print("== 結果：PASS=%d FAIL=%d ==" % (_pass, _fail))
    return 0 if _fail == 0 else 1
