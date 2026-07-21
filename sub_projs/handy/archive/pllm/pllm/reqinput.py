"""reqinput.py — 把 CLI 的請求類旗標驗證／組成 core.ask 的請求輸入（cli.cpp 組請求段）。

⚠ 與 cllm 的 C++ llm 刻意分歧（只發生在 pllm）：--schema/--tool/--modality 的 cfg 收「字面 JSON
文字」（同 --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯
（退 1）。--image/--media 的三分流取值在 media.py（build_media_item）。本檔把這些旗標收成
core.ask 要的 schema／tools／modalities／media 四份輸入，並驗 --media-out 目錄。
"""
import json
import os
import sys

from .internal import EXIT_OK, EXIT_USAGE
from .media import build_media_item


class RequestInputs:
    """core.ask 的請求輸入四件組（schema／tools／modalities／media）。"""

    def __init__(self):
        self.schema_body = None
        self.tool_defs = []
        self.modality_items = []
        self.media_items = []


def build_request_inputs(p):
    """從 ParsedArgs 組請求輸入，回 (RequestInputs, EXIT_OK)；驗證失敗回 (None, 退出碼)。"""
    r = RequestInputs()

    if p.has_schema:
        try:
            json.loads(p.schema_text)  # 只驗合法；字面原樣交內核嵌入
        except Exception:
            sys.stderr.write("--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）\n")
            return None, EXIT_USAGE
        r.schema_body = p.schema_text

    for spec in p.tool_specs:  # 字面 tool def JSON；需 name 與非空 parameters（對齊 load_tool_def 語意）
        try:
            td = json.loads(spec)
        except Exception:
            sys.stderr.write("--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）\n")
            return None, EXIT_USAGE
        if not isinstance(td, dict) or not td.get("name") or not td.get("parameters"):
            sys.stderr.write("--tool 缺 name 或 parameters\n")
            return None, EXIT_USAGE
        r.tool_defs.append({"name": td["name"], "description": td.get("description", ""),
                            "parameters": td["parameters"]})

    for spec in p.modality_specs:  # 「名」或「名=<字面JSON>」
        name, eq, cfg = spec.partition("=")
        if not name:
            sys.stderr.write("--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）\n")
            return None, EXIT_USAGE
        if eq:  # 有 '='：cfg 收字面 JSON，驗合法
            try:
                json.loads(cfg)
            except Exception:
                sys.stderr.write("--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）\n")
                return None, EXIT_USAGE
        r.modality_items.append({"name": name, "config": cfg if eq else None})

    for spec in p.media_specs:  # 三分流：URL 直通 / .json 描述子直通 / 二進位圖檔讀檔編碼
        item = build_media_item(spec)
        if item is None:
            return None, EXIT_USAGE
        r.media_items.append(item)

    if p.media_out_dir and not os.path.isdir(p.media_out_dir):
        sys.stderr.write("--media-out 不是可用目錄：%s\n" % p.media_out_dir)
        return None, EXIT_USAGE

    return r, EXIT_OK
