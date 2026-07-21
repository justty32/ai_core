"""media.py — --image/--media 的 MIME 對照與三分流取值。

沿用已封存 pllm 的同名檔。--media 除了讀二進位圖檔，也能吃「已編碼」形式（data:／
http URL、或 .json 描述子），省掉每次重算 base64。
"""
import base64
import json
import os
import sys

_MIME = {"png": "image/png", "jpg": "image/jpeg", "jpeg": "image/jpeg",
         "gif": "image/gif", "webp": "image/webp", "wav": "audio/wav",
         "mp3": "audio/mpeg"}
_EXT = {"image/png": "png", "image/jpeg": "jpg", "image/gif": "gif",
        "image/webp": "webp", "audio/wav": "wav", "audio/mpeg": "mp3"}


def mime_of(path):
    """副檔名 → MIME。"""
    ext = os.path.splitext(path)[1].lstrip(".").lower()
    return _MIME.get(ext, "application/octet-stream")


def ext_of(mime):
    """MIME → 落檔副檔名。"""
    return _EXT.get(mime, "bin")


def _is_url(spec):
    low = spec.lower()
    if spec.startswith("data:"):
        return True
    return low.startswith("http://") or low.startswith("https://")


def _from_descriptor(spec):
    """.json 描述子 → media item；形狀不符或讀不到印 stderr 回 None。"""
    try:
        with open(spec, "rb") as f:
            desc = json.loads(f.read().decode("utf-8"))
    except OSError:
        sys.stderr.write("讀不到檔案：%s（--image/--media 描述子）\n" % spec)
        return None
    except Exception:                    # noqa: BLE001 —— JSON 壞了
        sys.stderr.write("media 描述子 JSON 解析失敗（%s）\n" % spec)
        return None

    if not isinstance(desc, dict):
        desc = {}
    if desc.get("url"):
        return {"url": desc["url"]}      # 直通 url、不編碼
    if desc.get("mime") and desc.get("data") is not None:
        try:
            raw = base64.b64decode(desc["data"])
        except Exception:                # noqa: BLE001
            sys.stderr.write("media 描述子的 data 不是合法 base64（%s）\n"
                             % spec)
            return None
        return {"url": "", "mime": desc["mime"], "bytes": raw}

    sys.stderr.write("media 描述子形狀不符——需 {\"url\":…} 或 "
                     "{\"mime\":…,\"data\":…}（%s）\n" % spec)
    return None


def build_media_item(spec):
    """--image/--media 一個值 → media item（三分流）。失敗印 stderr 回 None。

    1. data:／http(s):// URL → 直接當 url 送、不編碼。
    2. .json 副檔名（看副檔名、不嗅探內容）→ 當預先算好的描述子直通，接受
       {"url":…} 或 {"mime":…,"data":"<base64>"}。
    3. 其餘（二進位圖檔路徑）→ 讀檔，交由 call.py 編 base64。
    """
    if _is_url(spec):
        return {"url": spec}
    if spec.lower().endswith(".json"):
        return _from_descriptor(spec)
    try:
        with open(spec, "rb") as f:
            raw = f.read()
    except OSError:
        sys.stderr.write("讀不到檔案：%s（--image/--media）\n" % spec)
        return None
    return {"url": "", "mime": mime_of(spec), "bytes": raw}
