"""smoke.py — util.llm 的離線冒煙測試（不連網、不需要裝 litellm）。

跑法：python util/llm/test/smoke.py（從哪跑都行）。全過回 0，有失敗回 1。
管線（斷言／跑 CLI／記分）在 harness.py，本檔只放案例。

驗得到「argv 解析／退出碼分流／參數翻譯／回應解讀」；驗不到「litellm 真的打得通」
與「後端錯誤語意」——那要真後端。CLI 案例走 fixtures/ 的 file:// 假回應。
"""
import sys

from harness import (TEXT, TOOL, check, cli_case,   # noqa: E402
                     report, run_cli)
from util.llm.argv import parse_argv                # noqa: E402
from util.llm.call import (PLACEHOLDER_KEY, api_base,   # noqa: E402
                           build_kwargs, model_id)
from util.llm.cli import _make_prompt               # noqa: E402


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


def test_edge():
    """邊界：prompt 空掉的各種寫法、「-」在沒東西可讀時的分流。"""
    print("== 邊界 ==")
    cli_case("只有 --（其後全空）", ["--"], 1, "缺少 prompt")
    cli_case("只有 -，stdin 接上但空", ["-"], 1, "缺少 prompt")
    cli_case("只有 -，stdin 是互動終端", ["-"], 1, "互動終端", tty=True)
    cli_case("-- 之後只有一個 -", ["--", "-"], 1, "缺少 prompt")
    cli_case("空字串當 prompt", [""], 1, "缺少 prompt")
    cli_case("stdin 只有換行", [], 1, "缺少 prompt", stdin="\n\n")
    cli_case("--endpoint 缺值（在結尾）", ["--endpoint"], 1, "缺少值")
    cli_case("只有 - 且 stdin 有東西＝用 stdin",
             ["-", "--endpoint", TEXT], 0, "下不為例", stdin="資料")
    cli_case("-- 之後有內容就照用",
             ["--endpoint", TEXT, "--", "内容"], 0, "下不為例")
    # 旗標吃下一個 argv 當值，即使它長得像旗標（getopt 慣例）
    p, _ = parse_argv(["llm", "--model", "--stream", "hi"])
    check("旗標值長得像旗標也照吃", p.raw_values["model"][0], "--stream")
    check("被吃掉就不算旗標了", p.stream, False)


def test_prompt():
    """prompt 合體：「-」＝stdin 插入點，沒寫就接在後面（unix 慣例）。"""
    print("== prompt 與 stdin 合體 ==")
    check("「-」插在指定位置",
          _make_prompt(["請點評", "-", "這句詩"], "床前明月光"),
          "請點評 床前明月光 這句詩")
    check("沒寫「-」＝prompt＋空行＋stdin",
          _make_prompt(["請點評"], "床前明月光"),
          "請點評\n\n床前明月光")
    check("只有 stdin", _make_prompt([], "床前明月光"), "床前明月光")
    check("只有位置參數", _make_prompt(["只有這個"], ""), "只有這個")
    check("「-」但 stdin 是空的", _make_prompt(["前", "-", "後"], ""), "前  後")
    check("兩個「-」＝stdin 塞兩次",
          _make_prompt(["-", "-"], "資料"), "資料 資料")
    # 「--」只關掉旗標解析，不會讓「-」失效——兩者可共存（代價：傳不了字面的 -）
    p, _ = parse_argv(["llm", "--", "前", "-", "後"])
    check("「--」之後的「-」仍是 stdin 插入點", p.prompt_parts,
          ["前", "-", "後"])


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


if __name__ == "__main__":
    test_cli()
    test_edge()
    test_prompt()
    test_translate()
    sys.exit(report())
