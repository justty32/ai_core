"""core.py — cllm 的純 Python 內核：不碰 C ABI、不載 .so/.dll，直接打 OpenAI 相容 HTTP。

這是「真正做事的內核」——組請求、打 HTTP、解串流、跑回呼——與 C++ 的 `libcllm` 平行，
但完全走 Python 標準庫（`urllib` + `json`），零外部相依。API 形狀刻意對齊既有 ctypes
binding（`core/bindings/python/llm.py`），差別只在「不經 ctypes、直接打 HTTP」：

    from cllm import ask
    text = ask("你好")                                      # 只給 prompt（走內建 localhost）
    text = ask("你好", "http://…/chat/completions")         # prompt + endpoint（位置形式）
    text = ask("你好", model="local-model", temperature=0.7)
    ask("數到五", stream=True,                              # 串流：逐段進 on_delta
        on_delta=lambda piece: print(piece, end="", flush=True))   # 回真值可中止

ask 回「完整組合後的答案字串」；失敗時：給了 on_error 就呼叫它並回 None，否則 raise LLMError。
串流走 on_delta 逐段；回呼回真值＝要求中止（同 binding 慣例），中止後回「已收到的部分」。

tools／media／modalities（皆為 kwargs，可任意組合，等價於 C++ 的 build_body 產出）：

    ask("東京天氣如何？", tools=[{"name": "get_weather", "description": "查某城市天氣",
        "parameters": '{"type":"object","properties":{...}}'}],
        on_tool=lambda call: print(call["name"], call["arguments"]))  # call={id,name,arguments}
    ask("說句話", on_media=lambda m: print(m["mime"], len(m["bytes"])))  # m={mime,bytes}
    ask("描述這張圖", media=[{"url": "data:image/png;base64,…"}],
        modalities=[{"name": "audio", "config": '{"voice":"alloy"}'}])

⚠ 陷阱備忘：
  · 送出的請求 JSON 逐欄對齊 `core/src/cabi_request.cpp`（system role 排最前、content 無媒體＝
    字串／有媒體＝異質陣列、tools/schema/modalities 塞法）。用 compact separators＋ensure_ascii
    ＝False，好讓 LLM_DUMP_BODY 印出的 body 與 C++ 版逐字元一致（離線黑箱可比對）。
  · 串流解析在「bytes 層」按 '\\n' 拆框再逐行 decode——避免多位元組 UTF-8 字元被切在 chunk 邊界。
  · file://<絕對路徑> 特例照 `core/src/http.cpp` 的 do_file 規格：跳過 "file://" 後，若開頭是
    "/X:" 這種（Windows 的 file:///C:/…）去掉那個前導 '/'；POSIX 的 "/home/…" 保留。Windows + POSIX
    雙可跑，好讓 CLI 拿 test/fixtures 的假回應離線自測。
"""
import base64
import json
import os
import sys
import urllib.error
import urllib.request

# 內建預設 endpoint（對齊 core/src/cabi.cpp 的 make_request）。
DEFAULT_ENDPOINT = "http://localhost:1234/v1/chat/completions"


class LLMError(Exception):
    """傳輸／後端錯誤（未提供 on_error 時拋出；對應 binding 的 LLMError）。"""


# ── file:// 路徑正規化（逐字對齊 core/src/http.cpp 的 do_file）──
def _file_url_to_path(url):
    """把 file:// URL 轉成本機路徑。Windows(file:///C:/…) 去前導 '/'；POSIX(/home/…) 保留。"""
    path = url[7:]  # 跳過 "file://"
    if len(path) >= 3 and path[0] == "/" and path[2] == ":":
        path = path[1:]
    return path


def _b64decode(s):
    """寬鬆 base64 解碼（跳過換行／空白，對齊 C++ 的 base64_decode）。"""
    return base64.b64decode("".join(str(s).split()))


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


# ── 傳輸（統一 file:// 與真 HTTP，回 (status, chunk 迭代器)；對齊 http.cpp 的 run）──
def _transport(url, headers, body, timeout_ms):
    if url.startswith("file://"):
        path = _file_url_to_path(url)

        def gen_file():
            try:
                f = open(path, "rb")
            except OSError as e:
                raise LLMError("http: 開不了 file:// 路徑：" + path) from e
            with f:
                while True:
                    b = f.read(8192)
                    if not b:
                        break
                    yield b
        return 200, gen_file()

    timeout = (timeout_ms / 1000.0) if timeout_ms and timeout_ms > 0 else None
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        resp = urllib.request.urlopen(req, timeout=timeout)  # noqa: S310（url 由呼叫端控制）
    except urllib.error.HTTPError as e:  # 4xx/5xx：讀 body 交給 guard 處理錯誤語意
        data = e.read()
        return e.code, iter((data,))
    except urllib.error.URLError as e:  # 連不上／逾時：對齊 C++ 的「傳輸失敗」
        raise LLMError("http: 傳輸失敗——" + str(e.reason)) from e
    except OSError as e:
        raise LLMError("http: 傳輸失敗——" + str(e)) from e

    def gen_http():
        with resp:
            while True:
                b = resp.read(8192)
                if not b:
                    break
                yield b
    return (resp.getcode() or 200), gen_http()


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


def ask(prompt, endpoint=None, *, system=None, api_key=None, model=None, timeout_ms=None,
        temperature=None, top_p=None, presence_penalty=None, frequency_penalty=None,
        max_tokens=None, seed=None, stream=False, schema=None,
        tools=None, on_tool=None, media=None, modalities=None, on_media=None,
        on_delta=None, on_error=None):
    """問 LLM，回完整答案字串。詳見模組 docstring。"""
    acc = []          # 收集所有文字段（回傳用）
    cb_exc = {}       # 回呼自己丟的例外（同 binding：事後原樣上拋）

    def wrap(user_cb):
        """把使用者回呼包成「回 bool 中止」＋收下例外的內部回呼。"""
        def inner(arg):
            if user_cb is None:
                return False
            try:
                return bool(user_cb(arg))
            except Exception as e:  # noqa: BLE001 —— 收下、乾淨中止後再上拋
                cb_exc["e"] = e
                return True
        return inner

    on_tool_i = wrap(on_tool)
    on_media_i = wrap(on_media)

    def on_text_i(piece):
        acc.append(piece)
        if on_delta is None:
            return False
        try:
            return bool(on_delta(piece))
        except Exception as e:  # noqa: BLE001
            cb_exc["e"] = e
            return True

    try:
        if prompt is None:
            raise LLMError("llm_ask: req/prompt 為空")
        body = _build_body(prompt, system=system, model=model, stream=stream,
                           temperature=temperature, top_p=top_p, max_tokens=max_tokens,
                           presence_penalty=presence_penalty, frequency_penalty=frequency_penalty,
                           seed=seed, schema=schema, tools=tools, media=media,
                           modalities=modalities)
        dump = os.environ.get("LLM_DUMP_BODY")
        if dump and dump != "0":  # 除錯／trace：把送出的請求 JSON 印到 stderr（對齊 C++）
            sys.stderr.write(body.decode("utf-8") + "\n")
            sys.stderr.flush()

        url = endpoint or DEFAULT_ENDPOINT
        headers = {"Content-Type": "application/json"}
        if api_key:
            headers["Authorization"] = "Bearer " + api_key

        status, chunks = _transport(url, headers, body, timeout_ms)
        if stream:
            _dispatch_stream(status, chunks, on_text_i, on_tool_i)
        else:
            raw = b"".join(chunks)
            _guard_backend_error(status, raw)
            _dispatch_nonstream(raw.decode("utf-8", "replace"), on_text_i, on_tool_i, on_media_i)
    except Exception as e:  # noqa: BLE001
        if "e" in cb_exc:    # 回呼丟的例外優先原樣上拋（同 binding 先查 cb）
            raise cb_exc["e"]
        if on_error is not None:
            try:
                on_error(str(e))
            except Exception:  # noqa: BLE001
                pass
            return None
        raise e if isinstance(e, LLMError) else LLMError(str(e))

    if "e" in cb_exc:
        raise cb_exc["e"]
    return "".join(acc)
