"""media.py — --image/--media 的 MIME 對照與三分流取值。

mime_of／ext_of 對齊 cli_config.cpp 的同名對照表。build_media_item 是 core-py 特有的取值分流：
讓 --media 除了讀二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次
重算 base64（見 README「用法」段）。
"""
import json
import os
import sys

from . import core
from .internal import read_file_bytes, read_file_text


def mime_of(path):
    """副檔名 → MIME（對齊 cli_config.cpp 的 mime_of）。"""
    ext = os.path.splitext(path)[1].lstrip(".").lower()
    return {"png": "image/png", "jpg": "image/jpeg", "jpeg": "image/jpeg",
            "gif": "image/gif", "webp": "image/webp", "wav": "audio/wav",
            "mp3": "audio/mpeg"}.get(ext, "application/octet-stream")


def ext_of(mime):
    """MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of）。"""
    return {"image/png": "png", "image/jpeg": "jpg", "image/gif": "gif",
            "image/webp": "webp", "audio/wav": "wav", "audio/mpeg": "mp3"}.get(mime, "bin")


def build_media_item(spec):
    """--image/--media 一個值 → core media item（三分流）。成功回 dict、失敗印 stderr 回 None。

    1. data:／http(s):// URL → 直接當 media 的 url 送、不編碼（帶 url 就不帶 bytes）。
    2. .json 副檔名（用副檔名判定、不嗅探內容）→ 讀該檔 json.loads，當「預先算好的 media 描述子」
       直通、不重算 base64。接受兩形：{"url": "data:…"} 或 {"mime": "…", "data": "<base64>"}。
    3. 其餘（二進位圖檔路徑）→ 讀檔 + 讓內核 base64 編碼（現行行為，不變）。
    """
    low = spec.lower()
    if spec.startswith("data:") or low.startswith("http://") or low.startswith("https://"):
        return {"url": spec}  # URL 直通
    if low.endswith(".json"):  # 預先算好的 media 描述子
        try:
            body = read_file_text(spec)
        except OSError:
            sys.stderr.write("讀不到檔案：%s（--image/--media 描述子）\n" % spec)
            return None
        try:
            desc = json.loads(body)
        except Exception:
            sys.stderr.write("media 描述子 JSON 解析失敗（%s）\n" % spec)
            return None
        if isinstance(desc, dict) and desc.get("url"):
            return {"url": desc["url"]}  # 直通 url、不編碼
        if isinstance(desc, dict) and desc.get("mime") and desc.get("data") is not None:
            try:
                raw = core._b64decode(desc["data"])  # decode 回 raw、走既有 bytes 路（不重讀原圖）
            except Exception:
                sys.stderr.write("media 描述子的 data 不是合法 base64（%s）\n" % spec)
                return None
            return {"url": "", "mime": desc["mime"], "bytes": raw}
        sys.stderr.write("media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（%s）\n" % spec)
        return None
    try:  # 二進位圖檔：讀檔 + 內核 base64
        raw = read_file_bytes(spec)
    except OSError:
        sys.stderr.write("讀不到檔案：%s（--image/--media）\n" % spec)
        return None
    return {"url": "", "mime": mime_of(spec), "bytes": raw}
