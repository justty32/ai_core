"""stream.py — SSE 串流解析。對齊 core/src/cabi_stream.cpp 的 do_stream：在 bytes 層按 '\\n'
拆 SSE 框再逐行 decode（避免多位元組 UTF-8 字元被切在 chunk 邊界），tool_calls 逐塊拼、
拼完整才交付。收尾複用 response.py 的 _guard_backend_error 判後端錯誤。
"""
import json

from .response import _guard_backend_error


# ── 串流解析（對齊 cabi_stream.cpp 的 do_stream：bytes 層拆 SSE 框、tool_calls 拼完整才交付）──
def _dispatch_stream(status, chunk_iter, on_text, on_tool):
    buf = b""
    raw_all = b""
    acc = []  # 每格 {id,name,arguments,used}

    def slot(idx):
        while len(acc) <= idx:
            acc.append({"id": "", "name": "", "arguments": "", "used": False})
        return acc[idx]

    aborted = False
    done = False
    for chunk in chunk_iter:
        raw_all += chunk
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            if line.endswith(b"\r"):
                line = line[:-1]
            if not line.startswith(b"data: "):
                continue
            data = line[6:]
            if data == b"[DONE]":
                done = True
                break
            try:
                obj = json.loads(data.decode("utf-8"))
            except Exception:
                continue
            choices = obj.get("choices") or []
            if not choices:
                continue
            delta = choices[0].get("delta") or {}
            content = delta.get("content")
            if content:
                if on_text and on_text(content):
                    aborted = True
                    break
            tcs = delta.get("tool_calls")
            if tcs:  # 逐塊拼：首塊帶 id/name，後續塊補 arguments 片段（index 對齊）
                for tc in tcs:
                    idx = tc.get("index")
                    idx = idx if idx is not None else len(acc)
                    s = slot(idx)
                    if tc.get("id"):
                        s["id"] = tc["id"]
                    fn = tc.get("function")
                    if fn:
                        if fn.get("name"):
                            s["name"] = fn["name"]
                        if fn.get("arguments") is not None:
                            s["arguments"] += fn["arguments"]
                    s["used"] = True
        if aborted or done:
            break

    if aborted:
        return
    _guard_backend_error(status, raw_all)  # 後端出錯（4xx／error JSON）→ raise
    if on_tool:  # tool_calls 一律拼完整才交付
        for a in acc:
            if not a["used"]:
                continue
            if on_tool({"id": a["id"], "name": a["name"], "arguments": a["arguments"]}):
                break
