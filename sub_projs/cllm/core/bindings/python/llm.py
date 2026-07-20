"""llm.py — cllm 的 Python binding：用 ctypes 直接載入 libcllm.so、呼叫 C ABI（唯一入口 llm_ask）。

純 Python、零編譯（ctypes 直接吃 .so）——最符合「小、免開發環境」。API 對齊 galtxt/try_4 慣例
（ask + on_delta 串流回呼；Python 用關鍵字參數）：

    import llm
    text = llm.ask("你好")                                  # 只給 prompt（走內建 localhost）
    text = llm.ask("你好", "http://…/chat/completions")     # prompt + endpoint（位置形式）
    text = llm.ask("你好", model="local-model", temperature=0.7)
    llm.ask("數到五", stream=True,                          # 串流：逐段進 on_delta
            on_delta=lambda piece: print(piece, end="", flush=True))   # 回真值可中止

ask 回「完整組合後的答案字串」；失敗時：給了 on_error 就呼叫它並回 None，否則 raise LLMError。

tools／media／modalities（v2 補齊，皆為 kwargs，可任意組合）：

    llm.ask("東京天氣如何？", tools=[{"name": "get_weather", "description": "查某城市天氣",
             "parameters": '{"type":"object","properties":{...}}'}],
            on_tool=lambda call: print(call["name"], call["arguments"]))  # call={id,name,arguments}
    llm.ask("說句話", on_media=lambda m: print(m["mime"], len(m["bytes"])))  # m={mime,bytes}
    llm.ask("描述這張圖", media=[{"url": "data:image/png;base64,…"}],
            modalities=[{"name": "audio", "config": '{"voice":"alloy"}'}])

on_tool／on_media 回真值＝要求中止（同 on_delta 慣例）。

消費穩定 C ABI（見 ../../docs/c-abi-reference.md）。libcllm.so 路徑：預設 ../../build/libcllm.so，
可用環境變數 LIBCLLM 覆寫。範圍：prompt＋連線/取樣選項＋stream＋schema＋on_delta/on_error＋
tools/on_tool＋media/on_media＋modalities。
"""
import ctypes
import os
from ctypes import (CDLL, CFUNCTYPE, POINTER, Structure, byref, string_at,
                    c_char_p, c_float, c_int, c_long, c_size_t, c_uint, c_void_p)

# ── field_mask 位旗標（對應 cabi_client.h 的 LLM_FIELD_*）──
_F_TEMPERATURE, _F_TOP_P, _F_PRESENCE, _F_FREQUENCY, _F_MAX_TOKENS, _F_SEED = (
    1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5)


class LLMError(Exception):
    """傳輸/後端錯誤（llm_ask 回 LLM_ERROR，且未提供 on_error 時拋出）。"""


# ── C ABI 結構鏡像（欄位順序/型別須與 cabi_*.h 完全一致）──
class _Client(Structure):
    _fields_ = [("endpoint", c_char_p), ("api_key", c_char_p), ("model", c_char_p),
                ("timeout_ms", c_long), ("field_mask", c_uint),
                ("temperature", c_float), ("top_p", c_float),
                ("presence_penalty", c_float), ("frequency_penalty", c_float),
                ("max_tokens", c_int), ("seed", c_int)]


class _Schema(Structure):
    _fields_ = [("name", c_char_p), ("json", c_char_p)]


# ── 輸入型（送出去給模型看的：tool 定義／輸入媒體／想要的輸出模態）──
class _ToolDef(Structure):
    _fields_ = [("name", c_char_p), ("description", c_char_p), ("parameters", c_char_p)]


class _MediaIn(Structure):
    _fields_ = [("url", c_char_p), ("mime", c_char_p), ("data", c_char_p), ("len", c_size_t)]


class _Modality(Structure):
    _fields_ = [("name", c_char_p), ("config", c_char_p)]


class _Request(Structure):
    _fields_ = [("prompt", c_char_p), ("schema", POINTER(_Schema)),
                ("tools", POINTER(_ToolDef)), ("tools_count", c_size_t),
                ("media", POINTER(_MediaIn)), ("media_count", c_size_t),
                ("modalities", POINTER(_Modality)), ("modalities_count", c_size_t),
                ("stream", c_int)]


# ── 收回型（模型吐回來的：tool 呼叫／產出媒體）──
# llm_tool_call_t 三欄皆 plain const char*（NUL 結尾字串，無需另帶長度）→ 直接用 c_char_p 讓
# ctypes 自動轉 bytes 即可。llm_media_out_t 的 data 是「原始位元組＋len」、不保證 NUL 結尾，
# 跟 on_text 的 text 同款陷阱，故同樣宣告成 c_void_p、讀取時自己用 string_at(ptr, len) 精確取。
class _ToolCall(Structure):
    _fields_ = [("id", c_char_p), ("name", c_char_p), ("arguments", c_char_p)]


class _MediaOut(Structure):
    _fields_ = [("mime", c_char_p), ("data", c_void_p), ("len", c_size_t)]


# handler 函數指標型（text/message 走 c_void_p+len，用 string_at 精確取 bytes——不保證 NUL 結尾）
_TEXT_H = CFUNCTYPE(c_int, c_void_p, c_size_t, c_void_p)
_TOOL_H = CFUNCTYPE(c_int, POINTER(_ToolCall), c_void_p)
_MEDIA_H = CFUNCTYPE(c_int, POINTER(_MediaOut), c_void_p)
_ERROR_H = CFUNCTYPE(None, c_void_p, c_size_t, c_void_p)


class _Handlers(Structure):
    _fields_ = [("on_text", _TEXT_H), ("text_user", c_void_p),
                ("on_tool", _TOOL_H), ("tool_user", c_void_p),
                ("on_media", _MEDIA_H), ("media_user", c_void_p),
                ("on_error", _ERROR_H), ("error_user", c_void_p)]


_LIB = None


def _lib():
    global _LIB
    if _LIB is None:
        here = os.path.dirname(os.path.abspath(__file__))
        default = os.path.join(here, "..", "..", "build", "libcllm.so")
        path = os.environ.get("LIBCLLM", default)
        lib = CDLL(path)
        lib.llm_ask.restype = c_int
        lib.llm_ask.argtypes = [c_void_p, POINTER(_Client), POINTER(_Request), POINTER(_Handlers)]
        _LIB = lib
    return _LIB


def ask(prompt, endpoint=None, *, api_key=None, model=None, timeout_ms=None,
        temperature=None, top_p=None, presence_penalty=None, frequency_penalty=None,
        max_tokens=None, seed=None, stream=False, schema=None,
        tools=None, on_tool=None, media=None, modalities=None, on_media=None,
        on_delta=None, on_error=None):
    """問 LLM，回完整答案字串。詳見模組 docstring。"""
    lib = _lib()
    keep = []  # 讓傳給 C 的 bytes / struct 陣列 / callback 在 llm_ask 期間不被 GC

    def cstr(s):
        if s is None:
            return None
        b = s.encode("utf-8")
        keep.append(b)
        return b

    def cbytes(b):
        """跟 cstr 一樣顧生命週期，但吃已經是 bytes 的原始位元組（不經編碼）。"""
        if b is None:
            return None
        keep.append(b)
        return b

    def build_tools(items):
        if not items:
            return None, 0
        arr = (_ToolDef * len(items))()
        for i, t in enumerate(items):
            arr[i].name = cstr(t.get("name"))
            arr[i].description = cstr(t.get("description"))
            arr[i].parameters = cstr(t.get("parameters"))
        keep.append(arr)
        return arr, len(items)

    def build_media(items):
        if not items:
            return None, 0
        arr = (_MediaIn * len(items))()
        for i, m in enumerate(items):
            arr[i].url = cstr(m.get("url"))
            arr[i].mime = cstr(m.get("mime"))
            raw = m.get("bytes")
            if raw is not None:
                if isinstance(raw, str):
                    raw = raw.encode("utf-8")
                arr[i].data = cbytes(raw)
                arr[i].len = len(raw)
            else:
                arr[i].data = None
                arr[i].len = 0
        keep.append(arr)
        return arr, len(items)

    def build_modalities(items):
        if not items:
            return None, 0
        arr = (_Modality * len(items))()
        for i, m in enumerate(items):
            arr[i].name = cstr(m.get("name"))
            arr[i].config = cstr(m.get("config"))
        keep.append(arr)
        return arr, len(items)

    c = _Client()
    c.endpoint = cstr(endpoint)
    c.api_key = cstr(api_key)
    c.model = cstr(model)
    c.timeout_ms = int(timeout_ms) if timeout_ms is not None else 0
    mask = 0
    for val, field, bit in ((temperature, "temperature", _F_TEMPERATURE),
                            (top_p, "top_p", _F_TOP_P),
                            (presence_penalty, "presence_penalty", _F_PRESENCE),
                            (frequency_penalty, "frequency_penalty", _F_FREQUENCY),
                            (max_tokens, "max_tokens", _F_MAX_TOKENS),
                            (seed, "seed", _F_SEED)):
        if val is not None:
            setattr(c, field, val)
            mask |= bit
    c.field_mask = mask

    r = _Request()
    r.prompt = cstr(prompt)
    r.stream = 1 if stream else 0
    sc = _Schema()
    if schema is not None:
        sc.name = cstr("response")
        sc.json = cstr(schema)
        r.schema = ctypes.pointer(sc)
        keep.append(sc)
    r.tools, r.tools_count = build_tools(tools)
    r.media, r.media_count = build_media(media)
    r.modalities, r.modalities_count = build_modalities(modalities)

    acc = bytearray()
    state = {}  # 'cb'=回呼丟的例外, 'msg'=錯誤訊息

    @_TEXT_H
    def on_text(text, length, _user):
        chunk = string_at(text, length)
        acc.extend(chunk)
        if on_delta is not None:
            try:
                abort = on_delta(chunk.decode("utf-8", "replace"))
            except Exception as e:  # noqa: BLE001 —— 收下回呼例外、乾淨中止後再上拋
                state["cb"] = e
                return 1
            return 1 if abort else 0
        return 0

    @_TOOL_H
    def on_tool_cb(call, _user):
        if not call:
            return 0
        tc = call.contents
        d = {"id": tc.id.decode("utf-8", "replace") if tc.id else "",
             "name": tc.name.decode("utf-8", "replace") if tc.name else "",
             "arguments": tc.arguments.decode("utf-8", "replace") if tc.arguments else ""}
        if on_tool is not None:
            try:
                abort = on_tool(d)
            except Exception as e:  # noqa: BLE001
                state["cb"] = e
                return 1
            return 1 if abort else 0
        return 0

    @_MEDIA_H
    def on_media_cb(media_out, _user):
        if not media_out:
            return 0
        mo = media_out.contents
        d = {"mime": mo.mime.decode("utf-8", "replace") if mo.mime else "",
             "bytes": string_at(mo.data, mo.len) if mo.data else b""}
        if on_media is not None:
            try:
                abort = on_media(d)
            except Exception as e:  # noqa: BLE001
                state["cb"] = e
                return 1
            return 1 if abort else 0
        return 0

    @_ERROR_H
    def on_err(msg, length, _user):
        m = string_at(msg, length).decode("utf-8", "replace")
        state["msg"] = m
        if on_error is not None:
            try:
                on_error(m)
            except Exception:  # noqa: BLE001
                pass

    h = _Handlers()
    h.on_text = on_text
    h.on_tool = on_tool_cb
    h.on_media = on_media_cb
    h.on_error = on_err
    keep.extend([on_text, on_tool_cb, on_media_cb, on_err])

    st = lib.llm_ask(None, byref(c), byref(r), byref(h))

    if "cb" in state:            # on_delta 自己丟了例外 → 原樣上拋
        raise state["cb"]
    if st == 0:                  # LLM_OK
        return acc.decode("utf-8", "replace")
    if st == -1:                 # LLM_CANCELLED（on_delta 回真值主動中止）→ 回已收到的部分
        return acc.decode("utf-8", "replace")
    if on_error is not None:     # LLM_ERROR，但 on_error 已被呼叫
        return None
    raise LLMError(state.get("msg", "llm_ask failed"))
