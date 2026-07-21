"""request.py — 請求組裝：把 prompt／system／tools／media／modalities 組成 OpenAI 相容
的 /chat/completions 請求 JSON（bytes）。對齊 core/src/cabi_request.cpp 的 build_body——
本檔即 pllm 這條線的「唯一請求真相源」。

⚠ 陷阱備忘：
  · 送出的請求 JSON 逐欄對齊 cabi_request.cpp：system role 排最前、content 無媒體＝字串／
    有媒體＝異質陣列、tools/schema/modalities 塞法。
  · 用 compact separators＋ensure_ascii=False：與 C++ glaze 的 body 逐字元對齊，好讓
    LLM_DUMP_BODY 印出的 body 與 C++ 版一致（離線黑箱可比對）。
"""
import base64
import json


# ── 請求組裝（等價於 core/src/cabi_request.cpp 的 build_body）──
def _as_obj(x):
    """把「JSON 物件」統一成 Python dict：字串→json.loads；已是 dict→原樣；None→{}。"""
    if x is None:
        return {}
    if isinstance(x, str):
        return json.loads(x) if x.strip() else {}
    return x


def _build_content(prompt, media):
    """message.content：無 media → 純字串；有 media → [文字格, 媒體格…]（對齊 build_content）。"""
    prompt = prompt or ""
    if not media:
        return prompt
    parts = [{"type": "text", "text": prompt}]
    for m in media:
        mime = m.get("mime") or ""
        url = m.get("url") or ""
        raw = m.get("bytes")
        if isinstance(raw, str):
            raw = raw.encode("utf-8")
        if mime.startswith("audio/") and raw:  # 音訊輸入：input_audio{data(base64), format}
            parts.append({"type": "input_audio", "input_audio": {
                "data": base64.b64encode(raw).decode("ascii"), "format": mime[6:]}})
        else:  # 其餘（圖）：image_url，url 直用或 data → data URI
            if url:
                u = url
            elif raw:
                mt = mime or "application/octet-stream"
                u = "data:" + mt + ";base64," + base64.b64encode(raw).decode("ascii")
            else:
                u = ""
            parts.append({"type": "image_url", "image_url": {"url": u}})
    return parts


def _build_body(prompt, *, system, model, stream, temperature, top_p, max_tokens,
                presence_penalty, frequency_penalty, seed, schema, tools, media, modalities):
    """組完整 OpenAI /chat/completions 請求 JSON（bytes）。欄位順序對齊 C++ 的 ReqBody。"""
    body = {}
    if model is not None:
        body["model"] = model
    messages = []
    if system:  # system role（若非空）排最前：OpenAI 慣例 system→user
        messages.append({"role": "system", "content": system})
    messages.append({"role": "user", "content": _build_content(prompt, media)})
    body["messages"] = messages
    body["stream"] = bool(stream)
    if temperature is not None:
        body["temperature"] = temperature
    if top_p is not None:
        body["top_p"] = top_p
    if max_tokens is not None:
        body["max_tokens"] = max_tokens
    if presence_penalty is not None:
        body["presence_penalty"] = presence_penalty
    if frequency_penalty is not None:
        body["frequency_penalty"] = frequency_penalty
    if seed is not None:
        body["seed"] = seed
    if schema is not None:  # 結構化輸出：response_format.json_schema
        body["response_format"] = {"type": "json_schema", "json_schema": {
            "name": "response", "strict": True, "schema": _as_obj(schema)}}
    if tools:
        body["tools"] = [{"type": "function", "function": {
            "name": t.get("name", ""), "description": t.get("description", "") or "",
            "parameters": _as_obj(t.get("parameters"))}} for t in tools]
    if modalities:  # names 進 modalities 陣列；有 config 者另拼成頂層鍵（如 "audio":{…}）
        names, extra = [], {}
        for mo in modalities:
            name = mo.get("name") or ""
            if name:
                names.append(name)
            cfg = mo.get("config")
            if cfg and name:
                extra[name] = _as_obj(cfg)
        if names:
            body["modalities"] = names
        for k, v in extra.items():
            body[k] = v
    # compact separators＋ensure_ascii=False：與 C++ glaze 的 body 逐字元對齊
    return json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
