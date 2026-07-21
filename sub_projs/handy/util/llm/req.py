"""req.py — 請求類旗標的驗證與組裝（沿用已封存 pllm 的 reqinput.py）。

--schema／--tool／--modality 收「字面 JSON 文字」不讀檔（同 --system）；要吃檔案內容
一律靠 shell `$(cat s.json)`。解 JSON 失敗＝用法錯。--image/--media 例外保留讀檔，
取值三分流在 media.py。本檔把這些旗標收成 ask() 要的四份請求輸入。
"""
import json
import os
import sys

from .errors import EXIT_OK, EXIT_USAGE
from .media import build_media_item


class RequestInputs:
    """ask() 的請求輸入四件組（schema／tools／modalities／media）。"""

    def __init__(self):
        self.schema_body = None
        self.tool_defs = []
        self.modality_items = []
        self.media_items = []


def _take_schema(p, r):
    """--schema：只驗合法 JSON，字面原樣交內核嵌入。"""
    try:
        json.loads(p.schema_text)
    except Exception:               # noqa: BLE001
        sys.stderr.write("--schema 不是合法 JSON（收字面文字非路徑；"
                         "長內容用 $(cat s.json)）\n")
        return False
    r.schema_body = p.schema_text
    return True


def _take_tools(p, r):
    """--tool：字面 JSON，需有 name 與非空 parameters。"""
    for spec in p.tool_specs:
        try:
            td = json.loads(spec)
        except Exception:           # noqa: BLE001
            sys.stderr.write("--tool 不是合法 JSON（收字面文字非路徑；"
                             "長內容用 $(cat t.json)）\n")
            return False
        if not isinstance(td, dict):
            td = {}
        if not td.get("name") or not td.get("parameters"):
            sys.stderr.write("--tool 缺 name 或 parameters\n")
            return False
        r.tool_defs.append({"name": td["name"],
                            "description": td.get("description", ""),
                            "parameters": td["parameters"]})
    return True


def _take_modalities(p, r):
    """--modality：「名」或「名=<字面JSON>」。"""
    for spec in p.modality_specs:
        name, eq, cfg = spec.partition("=")
        if not name:
            sys.stderr.write("--modality 缺模態名"
                             "（如 audio 或 audio={\"voice\":\"alloy\"}）\n")
            return False
        if eq:
            try:
                json.loads(cfg)
            except Exception:       # noqa: BLE001
                sys.stderr.write("--modality 的 config 不是合法 JSON"
                                 "（收字面文字；長內容用 $(cat cfg.json)）\n")
                return False
        if eq:
            r.modality_items.append({"name": name, "config": cfg})
        else:
            r.modality_items.append({"name": name, "config": None})
    return True


def build_request_inputs(p):
    """從 ParsedArgs 組請求輸入，回 (RequestInputs, EXIT_OK)；失敗回 (None, 碼)。"""
    r = RequestInputs()

    if p.has_schema and not _take_schema(p, r):
        return None, EXIT_USAGE
    if not _take_tools(p, r):
        return None, EXIT_USAGE
    if not _take_modalities(p, r):
        return None, EXIT_USAGE

    for spec in p.media_specs:
        item = build_media_item(spec)
        if item is None:
            return None, EXIT_USAGE
        r.media_items.append(item)

    if p.media_out_dir and not os.path.isdir(p.media_out_dir):
        sys.stderr.write("--media-out 不是可用目錄：%s\n" % p.media_out_dir)
        return None, EXIT_USAGE

    return r, EXIT_OK
