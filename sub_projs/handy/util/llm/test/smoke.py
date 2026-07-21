"""smoke.py — util.llm 的離線冒煙測試（不連網、不需要裝 litellm）。

跑法：python util/llm/test/smoke.py（從 handy 根跑）。全過回 0，有失敗回 1。

驗得到「argv 解析／退出碼分流／參數翻譯／回應解讀」；驗不到「litellm 真的打得通」
與「後端錯誤語意」——那要真後端。CLI 案例走 fixtures/ 的 file:// 假回應。
"""
import io
import os
import sys
from contextlib import redirect_stderr, redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(HERE))))

from util.llm import cli_main                        # noqa: E402
from util.llm.call import (PLACEHOLDER_KEY, api_base,        # noqa: E402
                           build_kwargs, model_id)

FIX = os.path.join(HERE, "fixtures")
TEXT = "file://" + os.path.join(FIX, "text.json").replace("\\", "/")
TOOL = "file://" + os.path.join(FIX, "tool.json").replace("\\", "/")

RESULT = {"pass": 0, "fail": 0}


def check(desc, got, want):
    """單一斷言。"""
    if got == want:
        print("  PASS  " + desc)
        RESULT["pass"] += 1
        return
    print("  FAIL  " + desc)
    print("        got =%r" % (got,))
    print("        want=%r" % (want,))
    RESULT["fail"] += 1


def run_cli(args):
    """跑一次 cli_main，回 (退出碼, stdout+stderr)。stdin 給空的、不卡住。"""
    out = io.StringIO()
    err = io.StringIO()
    saved = sys.stdin
    sys.stdin = io.StringIO("")
    try:
        with redirect_stdout(out), redirect_stderr(err):
            code = cli_main(args)
    finally:
        sys.stdin = saved
    return code, out.getvalue() + err.getvalue()


def cli_case(desc, args, want_code, want_in=""):
    """跑一個 CLI 案例，比對退出碼與輸出關鍵字。"""
    code, text = run_cli(args)
    if code == want_code and (not want_in or want_in in text):
        print("  PASS  " + desc)
        RESULT["pass"] += 1
        return
    print("  FAIL  %s（退出碼 want=%d got=%d）" % (desc, want_code, code))
    print("        " + text.strip().replace("\n", " | ")[:120])
    RESULT["fail"] += 1


def test_cli():
    """CLI 外殼：正常出口與用法錯分流。"""
    print("== CLI ==")
    cli_case("--help 印用法", ["--help"], 0, "連線／取樣旗標")
    cli_case("純文字回應", ["你好", "--endpoint", TEXT], 0, "下不為例")
    cli_case("tool_calls 一行一則 JSON", ["天氣", "--endpoint", TOOL], 0,
             '"tool":"get_weather"')
    cli_case("未知旗標", ["--bogus", "hi"], 1, "未知旗標")
    cli_case("-- 之後一律當 prompt", ["--endpoint", TEXT, "--", "--bogus"], 0,
             "下不為例")
    cli_case("第一個參數就被解析（不當程式名跳過）",
             ["--endpoint", TEXT, "hi"], 0, "下不為例")
    cli_case("旗標缺值", ["--model"], 1, "缺少值")
    cli_case("缺 prompt", [], 1, "缺少 prompt")
    cli_case("--tool 壞 JSON", ["--tool", "nope", "hi"], 1, "不是合法 JSON")
    cli_case("--schema 壞 JSON", ["--schema", "nope", "hi"], 1,
             "不是合法 JSON")
    cli_case("--tool 缺 parameters", ["--tool", '{"name":"t"}', "hi"], 1,
             "缺 name 或 parameters")
    cli_case("型別錯", ["--temperature", "abc", "hi"], 1, "需要小數")
    cli_case("--media-out 壞目錄", ["hi", "--media-out", "/nope/nope"], 1,
             "不是可用目錄")
    cli_case("讀不到媒體檔", ["hi", "--image", "/nope.png"], 1, "讀不到檔案")
    cli_case("打不通的端點＝請求失敗", ["hi", "--endpoint",
                                       "http://127.0.0.1:9/v1"], 2,
             "請求失敗")


def test_translate():
    """去程翻譯：endpoint／model／取樣／tools／schema／modalities／media。"""
    print("== 參數翻譯（去程）==")
    check("剝 /chat/completions",
          api_base("https://api.deepseek.com/v1/chat/completions"),
          "https://api.deepseek.com/v1")
    check("沒尾巴就原樣", api_base("http://h/v1"), "http://h/v1")
    check("model 加 openai/", model_id("deepseek-chat"),
          "openai/deepseek-chat")
    check("含斜線也加", model_id("google/gemma-4-e4b"),
          "openai/google/gemma-4-e4b")
    check("已帶前綴不重加", model_id("openai/gpt-4o"), "openai/gpt-4o")

    kw = build_kwargs("你好", "http://localhost:1234/v1/chat/completions", {
        "system": "你是貓", "api_key": "K", "model": "m",
        "timeout_ms": 120000, "temperature": 0.7, "max_tokens": None,
        "stream": True, "schema": '{"type":"object"}',
        "tools": [{"name": "t", "parameters": '{"type":"object"}'}],
        "modalities": [{"name": "audio", "config": '{"voice":"alloy"}'}],
        "media": [{"url": "data:image/png;base64,AA"}]})
    check("api_base", kw["api_base"], "http://localhost:1234/v1")
    check("timeout 毫秒→秒", kw["timeout"], 120.0)
    check("None 的取樣參數不送", "max_tokens" in kw, False)
    check("system 排最前", kw["messages"][0],
          {"role": "system", "content": "你是貓"})
    check("media → 異質 content", kw["messages"][1]["content"],
          [{"type": "text", "text": "你好"},
           {"type": "image_url",
            "image_url": {"url": "data:image/png;base64,AA"}}])
    check("tools 形狀", kw["tools"], [{"type": "function", "function": {
        "name": "t", "description": "", "parameters": {"type": "object"}}}])
    check("schema → response_format", kw["response_format"]["type"],
          "json_schema")
    check("modalities 名單", kw["modalities"], ["audio"])
    check("modality config 上頂層", kw["audio"], {"voice": "alloy"})

    plain = build_kwargs("嗨", "http://h/v1/chat/completions", {})
    check("無 media＝純字串 content", plain["messages"][0]["content"], "嗨")
    # 迴歸守門：沒 key 時必須補佔位，否則 litellm 連免認證的本機端點都打不出去
    check("沒 api_key 補佔位", plain["api_key"], PLACEHOLDER_KEY)
    check("有給就用使用者的", build_kwargs(
        "嗨", "http://h/v1", {"api_key": "K"})["api_key"], "K")


def main():
    test_cli()
    test_translate()
    total = RESULT["pass"] + RESULT["fail"]
    print("\n結果：%d/%d 通過" % (RESULT["pass"], total))
    return 1 if RESULT["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
