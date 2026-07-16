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

消費穩定 C ABI（見 ../../docs/c-abi-reference.md）。libcllm.so 路徑：預設 ../../build/libcllm.so，
可用環境變數 LIBCLLM 覆寫。範圍（v1，刻意小）：prompt＋連線/取樣選項＋stream＋schema＋on_delta/on_error；
tools／media／modalities 未收。
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


class _Request(Structure):
    _fields_ = [("prompt", c_char_p), ("schema", POINTER(_Schema)),
                ("tools", c_void_p), ("tools_count", c_size_t),
                ("media", c_void_p), ("media_count", c_size_t),
                ("modalities", c_void_p), ("modalities_count", c_size_t),
                ("stream", c_int)]


# handler 函數指標型（text/message 走 c_void_p+len，用 string_at 精確取 bytes——不保證 NUL 結尾）
_TEXT_H = CFUNCTYPE(c_int, c_void_p, c_size_t, c_void_p)
_TOOL_H = CFUNCTYPE(c_int, c_void_p, c_void_p)
_MEDIA_H = CFUNCTYPE(c_int, c_void_p, c_void_p)
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
        on_delta=None, on_error=None):
    """問 LLM，回完整答案字串。詳見模組 docstring。"""
    lib = _lib()
    keep = []  # 讓傳給 C 的 bytes / callback 在 llm_ask 期間不被 GC

    def cstr(s):
        if s is None:
            return None
        b = s.encode("utf-8")
        keep.append(b)
        return b

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
    h.on_error = on_err
    keep.extend([on_text, on_err])

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
