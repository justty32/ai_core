"""translate.py — OpenAI chat/completions ⇄ Anthropic Messages 的純翻譯層。

沒有網路、沒有伺服器、沒有 I/O——只有形狀轉換的純函數，方便離線測。
cllm 只會講 OpenAI 語＋Bearer；Anthropic 只收 Messages 語＋x-api-key。這裡把兩邊接起來：

    to_anthropic(oai_body)      OpenAI 請求 dict → Anthropic 請求 dict
    from_anthropic(anth_body)   Anthropic 非串流回應 dict → OpenAI 回應 dict
    err_to_openai(anth_err)     Anthropic 錯誤 dict → OpenAI {"error":{"message":…}}
    StreamXlate                 逐筆吃 Anthropic SSE data，吐 OpenAI SSE data

契約來源：cllm 的 cabi_request.cpp（送出）／cabi_response.cpp／cabi_stream.cpp（收回）。
"""

from __future__ import annotations

import json

# response_format(json_schema) 沒有 Messages 端的原生對應，改用「強制工具」實作：
# 塞一個這個名字的工具＋tool_choice 逼模型呼叫，回來時把它的 input 當成 content JSON 字串
# 交還——cllm 的 schema 路徑本就期待 content 是那串 JSON，於是無縫。
STRUCT_TOOL = "__cllm_structured_output__"

DEFAULT_MAX_TOKENS = 4096  # Anthropic 的 max_tokens 是必填；OpenAI 沒給時墊這個


# ─────────────────────────── 送出：OpenAI → Anthropic ───────────────────────────

def _data_uri(url: str) -> dict | None:
    """data:<mime>;base64,<payload> → Anthropic base64 image source；不是就回 None。"""
    if not url.startswith("data:"):
        return None
    head, _, payload = url.partition(",")
    mime = head[5:].split(";", 1)[0] or "application/octet-stream"
    return {"type": "base64", "media_type": mime, "data": payload}


def _content_to_blocks(content) -> list[dict]:
    """OpenAI message.content（字串或 part 陣列）→ Anthropic content blocks。"""
    if content is None:
        return []
    if isinstance(content, str):
        return [{"type": "text", "text": content}] if content else []
    blocks: list[dict] = []
    for part in content:
        t = part.get("type")
        if t == "text":
            blocks.append({"type": "text", "text": part.get("text", "")})
        elif t == "image_url":
            url = (part.get("image_url") or {}).get("url", "")
            src = _data_uri(url) or ({"type": "url", "url": url} if url else None)
            if src:
                blocks.append({"type": "image", "source": src})
        # input_audio：Anthropic Messages 不吃音訊輸入 → 靜默略過
    return blocks


def _assistant_to_blocks(msg: dict) -> list[dict]:
    """助理訊息可能同時有文字與 tool_calls → 文字塊＋tool_use 塊。"""
    blocks = _content_to_blocks(msg.get("content"))
    for tc in msg.get("tool_calls") or []:
        fn = tc.get("function") or {}
        try:
            args = json.loads(fn.get("arguments") or "{}")
        except (ValueError, TypeError):
            args = {}
        blocks.append({"type": "tool_use", "id": tc.get("id", ""),
                       "name": fn.get("name", ""), "input": args})
    return blocks


def _tool_result_to_blocks(msg: dict) -> list[dict]:
    """OpenAI role=tool 訊息 → Anthropic user 端的 tool_result 塊。"""
    c = msg.get("content")
    text = c if isinstance(c, str) else json.dumps(c)
    return [{"type": "tool_result",
             "tool_use_id": msg.get("tool_call_id", ""),
             "content": text}]


def _oai_tools_to_anthropic(tools: list) -> list[dict]:
    out = []
    for t in tools or []:
        fn = t.get("function") or {}
        out.append({"name": fn.get("name", ""),
                    "description": fn.get("description", ""),
                    "input_schema": fn.get("parameters") or {"type": "object"}})
    return out


def to_anthropic(oai: dict, default_max_tokens: int = DEFAULT_MAX_TOKENS) -> dict:
    """OpenAI chat/completions 請求 → Anthropic /v1/messages 請求。"""
    systems: list[str] = []
    messages: list[dict] = []
    for m in oai.get("messages") or []:
        role = m.get("role")
        if role == "system":
            c = m.get("content")
            systems.append(c if isinstance(c, str) else json.dumps(c))
        elif role == "assistant":
            messages.append({"role": "assistant", "content": _assistant_to_blocks(m)})
        elif role == "tool":
            messages.append({"role": "user", "content": _tool_result_to_blocks(m)})
        else:  # user（含未知 role，一律當 user）
            messages.append({"role": "user", "content": _content_to_blocks(m.get("content"))})

    body: dict = {
        "model": oai.get("model", ""),
        "messages": messages,
        # OpenAI max_tokens 選填、Anthropic 必填 → 沒給就墊預設
        "max_tokens": oai.get("max_tokens") or default_max_tokens,
    }
    if systems:
        body["system"] = "\n\n".join(systems)
    if oai.get("stream"):
        body["stream"] = True
    # 取樣：溫度／top_p 直通；presence/frequency_penalty、seed、modalities 無對應 → 丟棄
    if "temperature" in oai:
        body["temperature"] = oai["temperature"]
    if "top_p" in oai:
        body["top_p"] = oai["top_p"]

    tools = _oai_tools_to_anthropic(oai.get("tools") or [])

    # response_format(json_schema) → 強制工具
    rf = oai.get("response_format") or {}
    if rf.get("type") == "json_schema":
        schema = (rf.get("json_schema") or {}).get("schema") or {"type": "object"}
        tools.append({"name": STRUCT_TOOL,
                      "description": "Return the result strictly as this JSON object.",
                      "input_schema": schema})
        body["tool_choice"] = {"type": "tool", "name": STRUCT_TOOL}

    if tools:
        body["tools"] = tools
    return body


# ─────────────────────────── 收回：Anthropic → OpenAI ───────────────────────────

def from_anthropic(anth: dict) -> dict:
    """Anthropic 非串流回應 → OpenAI {"choices":[{"message":{…}}]}。

    強制工具（STRUCT_TOOL）的 input 序列化成 content JSON 字串（還原 schema 語意）；
    其餘 tool_use → tool_calls；text 塊串接成 content。
    """
    text_parts: list[str] = []
    tool_calls: list[dict] = []
    struct_json: str | None = None
    for block in anth.get("content") or []:
        bt = block.get("type")
        if bt == "text":
            text_parts.append(block.get("text", ""))
        elif bt == "tool_use":
            if block.get("name") == STRUCT_TOOL:
                struct_json = json.dumps(block.get("input") or {}, ensure_ascii=False)
            else:
                tool_calls.append({
                    "id": block.get("id", ""),
                    "type": "function",
                    "function": {"name": block.get("name", ""),
                                 "arguments": json.dumps(block.get("input") or {},
                                                         ensure_ascii=False)},
                })

    message: dict = {"role": "assistant"}
    if struct_json is not None:
        message["content"] = struct_json
    else:
        message["content"] = "".join(text_parts)
    if tool_calls:
        message["tool_calls"] = tool_calls

    return {
        "id": anth.get("id", "chatcmpl-proxy"),
        "object": "chat.completion",
        "model": anth.get("model", ""),
        "choices": [{"index": 0, "message": message,
                     "finish_reason": _stop_reason(anth.get("stop_reason"))}],
    }


def _stop_reason(anth_reason) -> str:
    return {"end_turn": "stop", "max_tokens": "length",
            "stop_sequence": "stop", "tool_use": "tool_calls"}.get(anth_reason, "stop")


def err_to_openai(anth_err: dict) -> dict:
    """Anthropic {"type":"error","error":{"type","message"}} → OpenAI 風格錯誤外殼。"""
    detail = anth_err.get("error") if isinstance(anth_err, dict) else None
    msg = (detail or {}).get("message") if isinstance(detail, dict) else None
    return {"error": {"message": msg or json.dumps(anth_err, ensure_ascii=False)}}


# ─────────────────────────── 串流：Anthropic SSE → OpenAI SSE ───────────────────────────

class StreamXlate:
    """把 Anthropic 的 SSE 事件逐筆翻成 OpenAI 的 SSE chunk。

    用法：對每一筆 Anthropic 的 `data: {...}` 取出 JSON，丟 feed()，拿回 0..N 條要往
    cllm 寫出的 `data: {...}\\n\\n` 字串（已含結尾空行）。串流結束時 feed 會吐 `data: [DONE]`。

    Anthropic 事件靠 data JSON 自帶的 "type" 分辨（event: 行可忽略）：
      content_block_start / content_block_delta / content_block_stop / message_stop / error
    """

    def __init__(self) -> None:
        self._struct_idx: set[int] = set()   # 哪些 block index 是強制工具（→當 content 吐）

    def feed(self, data: dict) -> list[str]:
        t = data.get("type")
        if t == "content_block_start":
            return self._on_start(data)
        if t == "content_block_delta":
            return self._on_delta(data)
        if t == "message_stop":
            return [_sse({"choices": [{"index": 0, "delta": {},
                                       "finish_reason": "stop"}]}), "data: [DONE]\n\n"]
        if t == "error":
            return [_sse(err_to_openai(data))]
        return []  # message_start / ping / content_block_stop / message_delta：無需轉

    def _on_start(self, data: dict) -> list[str]:
        idx = data.get("index", 0)
        block = data.get("content_block") or {}
        if block.get("type") == "tool_use":
            if block.get("name") == STRUCT_TOOL:
                self._struct_idx.add(idx)   # 之後這塊的 partial_json 當文字吐
                return []
            return [_sse({"choices": [{"index": 0, "delta": {"tool_calls": [{
                "index": idx, "id": block.get("id", ""), "type": "function",
                "function": {"name": block.get("name", ""), "arguments": ""},
            }]}}]})]
        return []

    def _on_delta(self, data: dict) -> list[str]:
        idx = data.get("index", 0)
        delta = data.get("delta") or {}
        dt = delta.get("type")
        if dt == "text_delta":
            return [_sse({"choices": [{"index": 0,
                                       "delta": {"content": delta.get("text", "")}}]})]
        if dt == "input_json_delta":
            frag = delta.get("partial_json", "")
            if idx in self._struct_idx:  # 強制工具：JSON 片段 → 當 content 串出
                return [_sse({"choices": [{"index": 0, "delta": {"content": frag}}]})]
            return [_sse({"choices": [{"index": 0, "delta": {"tool_calls": [{
                "index": idx, "function": {"arguments": frag}}]}}]})]
        return []


def _sse(obj: dict) -> str:
    return "data: " + json.dumps(obj, ensure_ascii=False) + "\n\n"
