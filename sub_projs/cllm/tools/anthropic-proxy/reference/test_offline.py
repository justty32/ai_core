#!/usr/bin/env python3
"""test_offline.py — anthropic-proxy 的離線煙霧測試（不連網、不起伺服器）。

只測 translate.py 的純形狀轉換：OpenAI→Anthropic 請求、Anthropic→OpenAI 回應／錯誤、
串流事件翻譯。跑：python3 test_offline.py，應全綠。
"""

from __future__ import annotations

import json

import translate as T


def check(name, cond):
    print(("  ok  " if cond else " FAIL ") + name)
    return bool(cond)


def test_req_basic():
    oai = {"model": "claude-sonnet-5",
           "messages": [{"role": "system", "content": "be terse"},
                        {"role": "user", "content": "hi"}],
           "temperature": 0.4, "max_tokens": 128, "stream": True}
    a = T.to_anthropic(oai)
    return all([
        check("model 直通", a["model"] == "claude-sonnet-5"),
        check("system 抽到頂層", a.get("system") == "be terse"),
        check("system 不留在 messages", all(m["role"] != "system" for m in a["messages"])),
        check("user content → text block",
              a["messages"][0]["content"] == [{"type": "text", "text": "hi"}]),
        check("temperature 直通", a["temperature"] == 0.4),
        check("max_tokens 直通", a["max_tokens"] == 128),
        check("stream 直通", a["stream"] is True),
    ])


def test_req_maxtokens_default():
    a = T.to_anthropic({"messages": [{"role": "user", "content": "x"}]}, default_max_tokens=777)
    return check("沒給 max_tokens → 墊預設", a["max_tokens"] == 777)


def test_req_image_data_uri():
    oai = {"messages": [{"role": "user", "content": [
        {"type": "text", "text": "look"},
        {"type": "image_url", "image_url": {"url": "data:image/png;base64,QUJD"}}]}]}
    blocks = T.to_anthropic(oai)["messages"][0]["content"]
    img = blocks[1]
    return all([
        check("圖 → image block", img["type"] == "image"),
        check("data URI → base64 source",
              img["source"] == {"type": "base64", "media_type": "image/png", "data": "QUJD"}),
    ])


def test_req_image_http_url():
    oai = {"messages": [{"role": "user", "content": [
        {"type": "image_url", "image_url": {"url": "https://x/y.png"}}]}]}
    src = T.to_anthropic(oai)["messages"][0]["content"][0]["source"]
    return check("http 圖 → url source", src == {"type": "url", "url": "https://x/y.png"})


def test_req_tools():
    oai = {"messages": [{"role": "user", "content": "go"}],
           "tools": [{"type": "function", "function": {
               "name": "add", "description": "sum", "parameters": {"type": "object"}}}]}
    tool = T.to_anthropic(oai)["tools"][0]
    return all([
        check("tool name 直通", tool["name"] == "add"),
        check("parameters → input_schema", tool["input_schema"] == {"type": "object"}),
    ])


def test_req_schema_to_forced_tool():
    schema = {"type": "object", "properties": {"n": {"type": "integer"}}}
    oai = {"messages": [{"role": "user", "content": "num"}],
           "response_format": {"type": "json_schema",
                               "json_schema": {"name": "r", "schema": schema}}}
    a = T.to_anthropic(oai)
    st = [t for t in a["tools"] if t["name"] == T.STRUCT_TOOL]
    return all([
        check("json_schema → 強制工具", len(st) == 1 and st[0]["input_schema"] == schema),
        check("tool_choice 逼呼叫",
              a["tool_choice"] == {"type": "tool", "name": T.STRUCT_TOOL}),
    ])


def test_req_tool_roundtrip():
    oai = {"messages": [
        {"role": "assistant", "content": None,
         "tool_calls": [{"id": "c1", "type": "function",
                         "function": {"name": "add", "arguments": '{"a":1}'}}]},
        {"role": "tool", "tool_call_id": "c1", "content": "2"}]}
    msgs = T.to_anthropic(oai)["messages"]
    return all([
        check("assistant tool_calls → tool_use",
              msgs[0]["content"][0] == {"type": "tool_use", "id": "c1",
                                        "name": "add", "input": {"a": 1}}),
        check("role=tool → user tool_result",
              msgs[1]["role"] == "user" and
              msgs[1]["content"][0]["type"] == "tool_result" and
              msgs[1]["content"][0]["tool_use_id"] == "c1"),
    ])


def test_resp_text():
    anth = {"id": "msg_1", "model": "claude-sonnet-5", "stop_reason": "end_turn",
            "content": [{"type": "text", "text": "hello"}]}
    o = T.from_anthropic(anth)
    msg = o["choices"][0]["message"]
    return all([
        check("text → content", msg["content"] == "hello"),
        check("end_turn → stop", o["choices"][0]["finish_reason"] == "stop"),
    ])


def test_resp_tool():
    anth = {"stop_reason": "tool_use", "content": [
        {"type": "tool_use", "id": "t1", "name": "add", "input": {"a": 1}}]}
    tc = T.from_anthropic(anth)["choices"][0]["message"]["tool_calls"][0]
    return all([
        check("tool_use → tool_calls", tc["function"]["name"] == "add"),
        check("input → arguments JSON 字串", json.loads(tc["function"]["arguments"]) == {"a": 1}),
    ])


def test_resp_struct():
    anth = {"stop_reason": "tool_use", "content": [
        {"type": "tool_use", "name": T.STRUCT_TOOL, "input": {"n": 5}}]}
    msg = T.from_anthropic(anth)["choices"][0]["message"]
    return all([
        check("強制工具 → content JSON", json.loads(msg["content"]) == {"n": 5}),
        check("強制工具不外露成 tool_calls", "tool_calls" not in msg),
    ])


def test_err():
    o = T.err_to_openai({"type": "error", "error": {"type": "x", "message": "boom"}})
    return check("Anthropic error → OpenAI error.message", o["error"]["message"] == "boom")


def _one(sse: str) -> dict:
    assert sse.startswith("data: ")
    return json.loads(sse[6:].strip())


def test_stream_text():
    x = T.StreamXlate()
    out = x.feed({"type": "content_block_delta", "index": 0,
                  "delta": {"type": "text_delta", "text": "hi"}})
    return check("text_delta → delta.content",
                 _one(out[0])["choices"][0]["delta"]["content"] == "hi")


def test_stream_tool():
    x = T.StreamXlate()
    start = x.feed({"type": "content_block_start", "index": 1,
                    "content_block": {"type": "tool_use", "id": "t1", "name": "add"}})
    delta = x.feed({"type": "content_block_delta", "index": 1,
                    "delta": {"type": "input_json_delta", "partial_json": '{"a":'}})
    tc_start = _one(start[0])["choices"][0]["delta"]["tool_calls"][0]
    tc_delta = _one(delta[0])["choices"][0]["delta"]["tool_calls"][0]
    return all([
        check("tool 起始 → tool_calls id/name", tc_start["function"]["name"] == "add"),
        check("input_json_delta → arguments 片段", tc_delta["function"]["arguments"] == '{"a":'),
        check("index 對齊", tc_delta["index"] == 1),
    ])


def test_stream_struct_as_content():
    x = T.StreamXlate()
    x.feed({"type": "content_block_start", "index": 0,
            "content_block": {"type": "tool_use", "name": T.STRUCT_TOOL}})
    out = x.feed({"type": "content_block_delta", "index": 0,
                  "delta": {"type": "input_json_delta", "partial_json": '{"n":5}'}})
    return check("強制工具串流 → 當 content 吐",
                 _one(out[0])["choices"][0]["delta"]["content"] == '{"n":5}')


def test_stream_done():
    out = T.StreamXlate().feed({"type": "message_stop"})
    return check("message_stop → [DONE]", out[-1].strip() == "data: [DONE]")


def main() -> int:
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    ok = True
    for t in tests:
        print("• " + t.__name__)
        ok = t() and ok
    print("\n全綠 ✅" if ok else "\n有紅 ❌")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
