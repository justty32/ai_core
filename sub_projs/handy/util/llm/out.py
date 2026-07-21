"""out.py — ask() 的四個出口回呼打包成 Sink（沿用已封存 pllm 的 output.py）。

把「輸出接線＋其共享狀態」收成一個物件：文字吐 stdout、tool_calls 一行一則 JSON、
產出媒體落檔 --media-out、錯誤吐 stderr。回呼一律回 False（不主動中止）。收尾狀態
由 cli.py 讀來定退出碼。
"""
import json
import os
import sys

from .media import ext_of


class Sink:
    """ask() 的 handlers 與其共享狀態。把 sink.on_* 傳給 ask 的對應回呼。"""

    def __init__(self, media_out_dir):
        self.media_out_dir = media_out_dir
        self.wrote_text = False   # 有無吐過文字（決定要不要補尾端換行）
        self.had_error = False    # on_error 被呼叫過＝請求真失敗
        self.media_err = False    # 媒體落檔失敗＝結果真掉了
        self.media_n = 0          # 已落檔媒體數（供編號檔名）

    def on_delta(self, piece):
        sys.stdout.write(piece)
        sys.stdout.flush()
        self.wrote_text = True
        return False

    def on_tool(self, call):
        """tool_calls 一行一則 JSON：{"tool","id","arguments"}。"""
        args = call["arguments"] or "{}"
        try:
            args_obj = json.loads(args)
        except Exception:         # noqa: BLE001 —— 後端理應給 JSON；壞了就原樣塞
            args_obj = args
        line = {"tool": call["name"], "id": call["id"], "arguments": args_obj}
        sys.stdout.write(json.dumps(line, ensure_ascii=False,
                                    separators=(",", ":")) + "\n")
        sys.stdout.flush()
        return False

    def on_media(self, m):
        if not self.media_out_dir:      # 沒地方放＝明說丟棄
            sys.stderr.write("收到產出媒體（%s，%d bytes）但沒給 --media-out，"
                             "已丟棄\n" % (m["mime"], len(m["bytes"])))
            return False
        self.media_n += 1
        name = "llm-media-%d.%s" % (self.media_n, ext_of(m["mime"]))
        path = os.path.join(self.media_out_dir, name)
        try:
            with open(path, "wb") as f:
                f.write(m["bytes"])
        except OSError:
            sys.stderr.write("媒體落檔失敗：%s\n" % path)
            self.media_err = True
            return False
        sys.stdout.write(path + "\n")
        sys.stdout.flush()
        return False

    def on_error(self, msg):
        self.had_error = True
        sys.stderr.write("請求失敗：%s\n" % msg)
