#!/usr/bin/env python3
"""cli_smoke.py — cllm core-py CLI 的離線黑箱煙霧測試（仿 core/test/cli_smoke.sh）。

精神同 shell 版：CLI 是個可跑的程式，最誠實的測法就是「真的用 subprocess 跑它、驗 stdout／
退出碼」。全程走 file:// 假回應——直接複用 core/test/fixtures，不連網、不吃真後端。跨平台
（Windows + POSIX）：file:// URL 由 fixtures 絕對路徑（正斜線）拼成，內核的 _file_url_to_path
兩平台都解得動。

用：`python test/cli_smoke.py`（無需先安裝；用 -m cllm 跑，靠 PYTHONPATH 找到 package）。
全過回 0，任一敗回 1。
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)  # core-py/（cllm package 所在）
FX_ROOT = os.path.abspath(os.path.join(PROJ, "..", "core", "test", "fixtures"))


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


_pass = 0
_fail = 0


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


def main():
    if not os.path.isdir(FX_ROOT):
        print("找不到 fixtures：%s" % FX_ROOT, file=sys.stderr)
        return 1
    print("== cllm core-py CLI 離線煙霧測試 ==")
    print("fixtures: %s" % FX_ROOT)

    # ── (1) happy path：輸出正確 ──
    print("-- 輸出正確 --")
    expect_out("才不是為你回答的", "位置參數當 prompt（非串流）",
               ["你好", "--endpoint", fx("fake")])
    expect_out("才不是為你回答的", "--stream 串流逐段",
               ["數到五", "--stream", "--endpoint", fx("fake_stream")])
    expect_out("才不是為你回答的", "沒位置參數 → 讀 stdin",
               ["--endpoint", fx("fake")], stdin="從管線\n")
    expect_out("才不是為你回答的", "prompt＋stdin 合體（指令在前、資料在後）",
               ["總結這份", "--endpoint", fx("fake")], stdin="附加資料\n")
    expect_out("才不是為你回答的", "「-」＝stdin 插入點",
               ["把", "-", "翻成英文", "--endpoint", fx("fake")], stdin="內容\n")
    expect_out("才不是為你回答的", "旗標在前 + prompt（--stream 你好）",
               ["--stream", "--endpoint", fx("fake_stream"), "你好"])
    expect_out("才不是為你回答的", "-- 分隔符（旗標在前 -- prompt）",
               ["--stream", "--endpoint", fx("fake_stream"), "--", "你好"])

    # --schema 收「字面 JSON 文字」（⚠ 與 C++ llm 分歧：不再讀檔；要吃檔案用 $(cat s.json)）
    expect_out('"name":"星野"', "--schema 收字面 JSON → 結構化輸出",
               ["給我角色", "--schema", '{"type":"object"}', "--endpoint", fx("fake_json")])

    # ── system role ──
    print("-- system role --")
    expect_body('"role":"system","content":"你是傲嬌貓"', "--system → body 首則為 system 訊息",
                ["--system", "你是傲嬌貓", "你好", "--endpoint", fx("fake")])
    expect_body('"role":"user"', "--system 後仍保留 user 訊息",
                ["--system", "你是傲嬌貓", "你好", "--endpoint", fx("fake")])
    expect_no_body('"role":"system"', "未給 --system → body 無 system 訊息",
                   ["你好", "--endpoint", fx("fake")])

    # ── tools / modalities（皆收字面 JSON）──
    print("-- tools / modalities --")
    tool_lit = ('{"name":"get_weather","description":"查天氣",'
                '"parameters":{"type":"object","properties":{"city":{"type":"string"}}}}')
    expect_out('"tool":"get_weather"', "--tool 收字面 JSON → tool_calls 一行 JSON 吐 stdout",
               ["東京天氣", "--tool", tool_lit, "--endpoint", fx("fake_tool")])
    expect_out('"city":"東京"', "--tool → arguments 原樣內嵌",
               ["東京天氣", "--tool", tool_lit, "--endpoint", fx("fake_tool")])

    outdir = HERE
    media_file = os.path.join(outdir, "llm-media-1.wav")
    if os.path.exists(media_file):
        os.remove(media_file)
    expect_out("llm-media-1.wav", "--modality 收字面 JSON＋--media-out → 媒體落檔、路徑吐 stdout",
               ["說你好", "--modality", 'audio={"voice":"alloy"}', "--media-out", outdir,
                "--endpoint", fx("fake_media")])
    try:
        with open(media_file, "rb") as f:
            content = f.read()
        if content == b"hello-wav-bytes":
            _ok("落檔內容正確（base64 已解碼）")
        else:
            _ng("落檔內容不對：<%r>" % content)
    except OSError:
        _ng("落檔不存在：%s" % media_file)
    expect_exit(0, "媒體但沒給 --media-out → 明說丟棄、不炸",
                ["說你好", "--endpoint", fx("fake_media")])

    # ── media 直通三分流（--image/--media）──
    print("-- media 直通三分流 --")
    # 2) data:/http(s):// URL → 直接當 url 送、不編碼
    expect_body('"url":"data:image/png;base64,BBBB"', "--media data: URL 直通（不編碼）",
                ["描述這張圖", "--media", "data:image/png;base64,BBBB", "--endpoint", fx("fake")])
    # 3a) .json 描述子 {"url": …} → 直通 url
    murl = os.path.join(HERE, "_smoke_media_url.json")
    with open(murl, "w", encoding="utf-8") as f:
        f.write('{"url":"data:image/png;base64,AAAA"}')
    expect_body('"url":"data:image/png;base64,AAAA"', "--media .json 描述子 {url} → 直通",
                ["描述這張圖", "--media", murl, "--endpoint", fx("fake")])
    # 3b) .json 描述子 {"mime","data"} → decode→bytes→內核重組 data URI（不重讀原圖）
    mbin = os.path.join(HERE, "_smoke_media_bin.json")
    with open(mbin, "w", encoding="utf-8") as f:
        f.write('{"mime":"image/png","data":"aGVsbG8="}')  # base64("hello") 可 round-trip
    expect_body('data:image/png;base64,aGVsbG8=', "--media .json 描述子 {mime,data} → 直通",
                ["描述這張圖", "--media", mbin, "--endpoint", fx("fake")])
    # 別名 --image 走同一分流
    expect_body('"url":"data:image/png;base64,CCCC"', "--image 別名亦支援 URL 直通",
                ["描述這張圖", "--image", "data:image/png;base64,CCCC", "--endpoint", fx("fake")])
    # 形狀不符的 .json 描述子 → 退出碼 1
    mbad = os.path.join(HERE, "_smoke_media_bad.json")
    with open(mbad, "w", encoding="utf-8") as f:
        f.write('{"foo":1}')
    expect_exit(1, "--media .json 描述子形狀不符 → 退出碼 1", ["你好", "--media", mbad])

    # ── 設定來源：config 檔 + env 指路 + 旗標覆寫 ──
    print("-- 設定來源 --")
    conf = os.path.join(HERE, "_smoke_conf.json")
    with open(conf, "w", encoding="utf-8") as f:
        f.write('{"endpoint":"%s","temperature":0.7}' % fx("fake"))
    expect_out("才不是為你回答的", "--config 提供 endpoint（無 --endpoint 旗標）",
               ["你好", "--config", conf])
    expect_out("才不是為你回答的", "env LLM_CLI_CONFIG 指路",
               ["你好"], extra_env={"LLM_CLI_CONFIG": conf})
    expect_out('"name":"星野"', "命令列旗標覆寫 config 的 endpoint",
               ["你好", "--config", conf, "--endpoint", fx("fake_json")])

    # ── 退出碼分流 ──
    print("-- 退出碼分流 --")
    expect_exit(0, "成功", ["你好", "--endpoint", fx("fake")])
    expect_exit(0, "--help", ["--help"])
    expect_exit(1, "未知旗標", ["你好", "--bogus"])
    expect_exit(1, "旗標缺值（--image）", ["你好", "--image"])
    expect_exit(1, "缺 prompt（stdin 空）", ["--endpoint", fx("fake")], stdin="")
    expect_exit(1, "「-」但 stdin 空（拼完仍空）", ["-", "--endpoint", fx("fake")], stdin="")
    expect_exit(1, "--config 讀不到", ["你好", "--config", "/no/such.json"])
    # 字面優先後：給路徑會被當字面文字、解 JSON 失敗 → 退出碼 1（刻意與 C++ llm 分歧）
    expect_exit(1, "--schema 給路徑（當字面、非法 JSON）", ["你好", "--schema", "/no/such.json"])
    expect_exit(1, "--schema 字面非法 JSON", ["你好", "--schema", "{bad json"])
    expect_exit(1, "--system 缺值", ["你好", "--system"])
    bad = os.path.join(HERE, "_smoke_bad.json")
    with open(bad, "w", encoding="utf-8") as f:
        f.write("{bad json")
    expect_exit(1, "config JSON 壞", ["你好", "--config", bad])
    expect_exit(1, "--tool 給路徑（當字面、非法 JSON）", ["你好", "--tool", "/no/such.json"])
    expect_exit(1, "--tool 字面缺 name/parameters", ["你好", "--tool", '{"description":"x"}'])
    expect_exit(1, "--modality config 字面非法 JSON", ["你好", "--modality", "audio=/no/such.json"])
    expect_exit(1, "--media .json 描述子讀不到", ["你好", "--media", "/no/such.json"])
    expect_exit(1, "--media-out 不是目錄", ["你好", "--media-out", "/no/such/dir"])
    expect_exit(1, "數值旗標型別錯（--temperature abc）",
                ["你好", "--temperature", "abc", "--endpoint", fx("fake")])
    expect_exit(2, "請求失敗（連不上真 endpoint）",
                ["你好", "--endpoint", "http://127.0.0.1:59999/v1/chat/completions",
                 "--timeout-ms", "800"])

    # 清理暫存
    for p in (murl, mbin, mbad, conf, bad, media_file):
        try:
            os.remove(p)
        except OSError:
            pass

    print("== 結果：PASS=%d FAIL=%d ==" % (_pass, _fail))
    return 0 if _fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
