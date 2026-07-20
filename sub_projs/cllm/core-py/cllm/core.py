"""core.py — cllm 純 Python 內核的出口：組請求→打 HTTP→解回應→跑回呼的一次 ask()。
對齊 core/src/cabi.cpp（唯一入口 llm_ask ＋ make_request）。

這是「真正做事的內核」的門面——本身只做接線，把各關注點分派給姊妹模組（皆對齊 C++ 分檔）：
  request.py（cabi_request.cpp）＝組請求 body；http.py（http.cpp）＝傳輸管子；
  response.py（cabi_response.cpp）＝非串流解析＋錯誤護欄；stream.py（cabi_stream.cpp）＝SSE；
  errors.py＝共用 LLMError／DEFAULT_ENDPOINT。API 形狀刻意對齊既有 ctypes binding
（core/bindings/python/llm.py），差別只在「不經 ctypes、直接打 HTTP」：

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
"""
import os
import sys

# 對外／相容再匯出：LLMError／DEFAULT_ENDPOINT 供 `from cllm import ...`；_b64decode 供 media.py；
# _file_url_to_path 保留 core.* 取用路徑（歷史相容）。
from .errors import DEFAULT_ENDPOINT, LLMError
from .http import _file_url_to_path, _transport  # noqa: F401 —— _file_url_to_path 為相容再匯出
from .request import _build_body
from .response import _b64decode, _dispatch_nonstream, _guard_backend_error  # noqa: F401
from .stream import _dispatch_stream

__all__ = ["ask", "LLMError", "DEFAULT_ENDPOINT"]


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
