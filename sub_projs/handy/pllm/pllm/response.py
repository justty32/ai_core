"""response.py — 非串流回應解析＋後端錯誤護欄。對齊 core/src/cabi_response.cpp 的
guard_backend_error 與 dispatch_nonstream。

錯誤護欄（guard）：4xx／帶 error 物件的回應 → raise LLMError（串流版由 stream.py 收尾時也
複用它）。非串流解析（dispatch_nonstream）：解 choices[0].message，文字進 on_text、
tool_calls 進 on_tool、audio 進 on_media。_b64decode 為寬鬆 base64 解碼小工具（audio 用）。
"""
import base64
import json

from .errors import LLMError


def _b64decode(s):
    """寬鬆 base64 解碼（跳過換行／空白，對齊 C++ 的 base64_decode）。"""
    return base64.b64decode("".join(str(s).split()))


# ── 後端錯誤護欄（對齊 cabi_response.cpp 的 guard_backend_error）──
def _guard_backend_error(status, raw):
    try:
        env = json.loads(raw)
    except Exception:
        env = None
    if isinstance(env, dict) and isinstance(env.get("error"), dict):
        msg = env["error"].get("message", "")
        raise LLMError("後端錯誤 (HTTP %d): %s" % (status, msg))
    if status >= 400:
        text = raw.decode("utf-8", "replace") if isinstance(raw, (bytes, bytearray)) else str(raw)
        raise LLMError("後端錯誤 (HTTP %d): %s" % (status, text[:300]))


# ── 非串流解析（對齊 cabi_response.cpp 的 dispatch_nonstream）──
def _dispatch_nonstream(raw_text, on_text, on_tool, on_media):
    try:
        parsed = json.loads(raw_text)
    except Exception:
        return
    choices = parsed.get("choices") or []
    if not choices:
        return
    msg = choices[0].get("message") or {}
    content = msg.get("content")
    if content and on_text and on_text(content):
        return
    tool_calls = msg.get("tool_calls")
    if tool_calls and on_tool:
        for tc in tool_calls:
            fn = tc.get("function") or {}
            call = {"id": tc.get("id") or "", "name": fn.get("name") or "",
                    "arguments": fn.get("arguments") or ""}
            if on_tool(call):
                return
    audio = msg.get("audio")
    if audio and audio.get("data") and on_media:
        b = _b64decode(audio["data"])
        mime = "audio/" + (audio.get("format") or "wav")
        on_media({"mime": mime, "bytes": b})
