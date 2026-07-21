"""msg.py — 組 OpenAI 形狀的 messages（system／user＋輸入媒體）。

call.py 的一塊：把 prompt／system／media 攤成 litellm 收的 messages 陣列。
媒體有兩形——音訊走 input_audio，其餘（圖）走 image_url，能給 url 就不重編 base64。
"""
import base64


def _b64(raw):
    return base64.b64encode(raw).decode("ascii")


def _media_part(item):
    """一份輸入媒體 → content 的一格。"""
    mime = item.get("mime") or ""
    url = item.get("url") or ""
    raw = item.get("bytes")
    if isinstance(raw, str):
        raw = raw.encode("utf-8")

    if mime.startswith("audio/") and raw:
        return {"type": "input_audio",
                "input_audio": {"data": _b64(raw), "format": mime[6:]}}
    if url:
        return {"type": "image_url", "image_url": {"url": url}}
    if raw:
        kind = mime or "application/octet-stream"
        data_uri = "data:" + kind + ";base64," + _b64(raw)
        return {"type": "image_url", "image_url": {"url": data_uri}}
    return {"type": "image_url", "image_url": {"url": ""}}


def build_content(prompt, media):
    """message.content：無 media→純字串；有 media→[文字格, 媒體格…]。"""
    prompt = prompt or ""
    if not media:
        return prompt
    parts = [{"type": "text", "text": prompt}]
    for item in media:
        parts.append(_media_part(item))
    return parts


def build_messages(prompt, system, media):
    """messages：system role（若有）排最前，其後 user（OpenAI 慣例）。"""
    messages = []
    if system:
        messages.append({"role": "system", "content": system})
    messages.append({"role": "user", "content": build_content(prompt, media)})
    return messages
