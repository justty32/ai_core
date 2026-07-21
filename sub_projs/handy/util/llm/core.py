"""core.py — ask()：一次發問的門面，回完整答案字串。

取代已封存的 pllm 內核（archive/pllm/：自寫 request／http／stream／response 四件）。
組請求／傳輸／解串流全交給 litellm；本檔只做接線，翻譯分兩段擺在姊妹模組：
call.py＝去程（參數→litellm kwargs）、resp.py＝回程（回應→回呼）、errors.py＝葉。

    text = ask("你好")                      # 走內建 localhost endpoint
    text = ask("你好", "http://…/v1/chat/completions", model="local-model")
    ask("數到五", stream=True, on_delta=lambda s: print(s, end=""))

失敗時：給了 on_error 就呼叫它並回 None，否則 raise LLMError。回呼回真值＝要求中止
（同 pllm 慣例），中止後回「已收到的部分」。與舊 pllm 的分歧見 call.py。
"""
from .call import build_kwargs, load_litellm
from .errors import DEFAULT_ENDPOINT, LLMError
from .resp import emit, emit_delta, read_fixture, to_dict

__all__ = ["ask", "LLMError", "DEFAULT_ENDPOINT"]


def _run_stream(resp, on_text, on_tool):
    """串流：逐 chunk 吐文字；tool_call 收齊後才吐（分段到齊才成一則）。"""
    slots = {}
    stopped = False
    for chunk in resp:
        if emit_delta(to_dict(chunk), on_text, slots):
            stopped = True
            break
    if stopped:
        return
    for index in sorted(slots):
        if on_tool(slots[index]):
            return


def ask(prompt, endpoint=None, *, system=None, api_key=None, model=None,
        timeout_ms=None, temperature=None, top_p=None, presence_penalty=None,
        frequency_penalty=None, max_tokens=None, seed=None, stream=False,
        schema=None, tools=None, on_tool=None, media=None, modalities=None,
        on_media=None, on_delta=None, on_error=None):
    """問 LLM，回完整答案字串。簽章與舊 pllm.ask 相同；詳見模組 docstring。"""
    acc = []          # 收集所有文字段（回傳用）
    cb_exc = {}       # 回呼自己丟的例外（同 pllm：事後原樣上拋）

    def wrap(user_cb):
        """使用者回呼 → 「回 bool 中止」＋收下例外的內部回呼。"""
        def inner(arg):
            if user_cb is None:
                return False
            try:
                return bool(user_cb(arg))
            except Exception as e:   # noqa: BLE001 —— 收下、中止後再上拋
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
        except Exception as e:       # noqa: BLE001
            cb_exc["e"] = e
            return True

    opts = {"system": system, "api_key": api_key, "model": model,
            "timeout_ms": timeout_ms, "temperature": temperature,
            "top_p": top_p, "presence_penalty": presence_penalty,
            "frequency_penalty": frequency_penalty, "max_tokens": max_tokens,
            "seed": seed, "stream": stream, "schema": schema, "tools": tools,
            "media": media, "modalities": modalities}

    try:
        if prompt is None:
            raise LLMError("ask: prompt 為空")
        url = endpoint or DEFAULT_ENDPOINT
        if url.startswith("file://"):        # 離線 fixture：不經 litellm
            emit(read_fixture(url), on_text_i, on_tool_i, on_media_i)
        else:
            litellm = load_litellm()
            resp = litellm.completion(**build_kwargs(prompt, url, opts))
            if stream:
                _run_stream(resp, on_text_i, on_tool_i)
            else:
                emit(to_dict(resp), on_text_i, on_tool_i, on_media_i)
    except Exception as e:           # noqa: BLE001
        if "e" in cb_exc:            # 回呼丟的例外優先原樣上拋（同 pllm）
            raise cb_exc["e"]
        if on_error is not None:
            try:
                on_error(str(e))
            except Exception:        # noqa: BLE001
                pass
            return None
        if isinstance(e, LLMError):
            raise
        raise LLMError(str(e))

    if "e" in cb_exc:
        raise cb_exc["e"]
    return "".join(acc)
