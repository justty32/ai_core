"""resp.py — 把 litellm 的回應翻回 pllm 形狀的四個回呼。

「介面翻譯」的回程（去程在 call.py）。litellm 回的是物件，這裡一律先 to_dict 成純
dict 再讀——離線 fixture（file:// 的假回應 JSON）本來就是 dict，兩條路因此共用同一
份解讀碼。回呼回真值＝要求中止，本檔照傳。
"""
import base64
import json
import os


def to_dict(obj):
    """litellm 回應物件 → 純 dict；取不出就當空回應。"""
    if isinstance(obj, dict):
        return obj
    fn = getattr(obj, "model_dump", None)
    if fn is None:
        fn = getattr(obj, "dict", None)
    if fn is None:
        return {}
    try:
        return fn()
    except Exception:      # noqa: BLE001 —— 取不出就當空回應
        return {}


def _first_choice(data):
    """取回應的第一個 choice；沒有或形狀不對就回空 dict。"""
    choices = data.get("choices")
    if not choices:
        return {}
    first = choices[0]
    if not isinstance(first, dict):
        return {}
    return first


def _tool_call(tc):
    """回應裡的一則 tool_call → 回呼收的 {id, name, arguments}。"""
    fn = tc.get("function") or {}
    return {"id": tc.get("id", ""),
            "name": fn.get("name", ""),
            "arguments": fn.get("arguments", "")}


def emit(data, on_text, on_tool, on_media):
    """非串流回應 dict → 三個回呼。任一回呼要求中止即回 True。"""
    msg = _first_choice(data).get("message") or {}

    text = msg.get("content")
    if text and on_text(text):
        return True

    for tc in (msg.get("tool_calls") or []):
        if on_tool(_tool_call(tc)):
            return True

    audio = msg.get("audio") or {}
    if audio.get("data"):
        mime = "audio/" + (audio.get("format") or "wav")
        raw = base64.b64decode(audio["data"])
        if on_media({"mime": mime, "bytes": raw}):
            return True

    return False


def emit_delta(data, on_text, slots):
    """串流 chunk dict → 文字回呼；tool_call 分段累積進 slots（收齊才吐）。"""
    delta = _first_choice(data).get("delta") or {}

    text = delta.get("content")
    if text and on_text(text):
        return True

    for tc in (delta.get("tool_calls") or []):
        index = tc.get("index") or 0
        if index not in slots:
            slots[index] = {"id": "", "name": "", "arguments": ""}
        slot = slots[index]
        if tc.get("id"):
            slot["id"] = tc["id"]
        fn = tc.get("function") or {}
        if fn.get("name"):
            slot["name"] = fn["name"]
        if fn.get("arguments"):
            slot["arguments"] += fn["arguments"]

    return False


def read_fixture(url):
    """`file://<路徑>` 離線逃生口：讀該檔當一份非串流回應 JSON。

    只為無後端自測（archive/pllm/test/fixtures/ 那批仍可用），非正常路徑。
    """
    path = url[len("file://"):]
    if os.name != "nt" and not path.startswith("/"):
        path = "/" + path
    with open(path, "rb") as f:
        return json.loads(f.read().decode("utf-8"))
