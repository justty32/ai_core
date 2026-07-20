#!/usr/bin/env python3
"""anthropic-proxy — 讓只會講 OpenAI＋Bearer 的 cllm 打得到 Anthropic。

cllm 硬編 `Authorization: Bearer <key>`＋OpenAI wire format；Anthropic 收 `x-api-key`＋
`anthropic-version`＋Messages 格式。中間擺這支本機轉發代理：cllm 以為在跟 OpenAI 講話，
代理偷偷翻成 Anthropic，回應再翻回去。cllm 的 C++ 核心一行不動。

    python3 proxy.py                    # 監聽 127.0.0.1:8787
    python3 proxy.py --port 9000        # 換埠
    python3 proxy.py --base https://…   # 換上游（預設 https://api.anthropic.com）

代理「無狀態」：key 從 cllm 送來的 Authorization header 當場轉手成 x-api-key，
代理自己不存、不落地任何金鑰。翻譯形狀全在 translate.py（可離線測）。

cllm 端只要把 config 指過來（見 README）：
    endpoint = http://127.0.0.1:8787/v1/chat/completions
    model    = claude-sonnet-5（或別的 Anthropic 模型 id）
    api_key  = sk-ant-...（你的 Anthropic key，照樣填 config，由代理轉手）
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import translate

BASE_URL = os.environ.get("ANTHROPIC_BASE_URL", "https://api.anthropic.com")
API_VERSION = os.environ.get("ANTHROPIC_VERSION", "2023-06-01")
MAX_TOKENS = int(os.environ.get("ANTHROPIC_MAX_TOKENS", str(translate.DEFAULT_MAX_TOKENS)))


def _bearer(headers) -> str:
    """從 Authorization: Bearer <key> 掏出 key（cllm 就這一種）。"""
    auth = headers.get("Authorization", "")
    return auth[7:].strip() if auth.startswith("Bearer ") else auth.strip()


def _anthropic_headers(key: str) -> dict:
    return {"content-type": "application/json",
            "x-api-key": key,
            "anthropic-version": API_VERSION}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"  # 要 chunked 串流就得 1.1

    def log_message(self, fmt, *args):  # 靜音預設每請求一行的 stderr 噪音
        pass

    def do_POST(self):
        # cllm 只打一個 endpoint；路徑不強求，指到本代理即視為 chat/completions。
        try:
            length = int(self.headers.get("Content-Length", 0))
            oai = json.loads(self.rfile.read(length) or b"{}")
        except (ValueError, TypeError) as e:
            return self._error(400, "壞掉的請求 JSON：%s" % e)

        key = _bearer(self.headers)
        if not key:
            return self._error(401, "缺 Authorization: Bearer <anthropic-key>——"
                                    "把 sk-ant-... 填進 cllm config 的 api_key。")

        body = translate.to_anthropic(oai, MAX_TOKENS)
        want_stream = bool(body.get("stream"))
        url = BASE_URL.rstrip("/") + "/v1/messages"
        req = urllib.request.Request(url, data=json.dumps(body).encode(),
                                     headers=_anthropic_headers(key), method="POST")
        try:
            if want_stream:
                self._proxy_stream(req)
            else:
                self._proxy_once(req)
        except urllib.error.HTTPError as e:  # 上游 4xx/5xx：翻成 OpenAI 錯誤外殼原樣回
            self._upstream_error(e)
        except urllib.error.URLError as e:
            self._error(502, "連不上上游 %s：%s" % (url, e.reason))

    # ── 非串流：一次收全、翻形狀、回 ──
    def _proxy_once(self, req):
        with urllib.request.urlopen(req) as resp:
            anth = json.loads(resp.read() or b"{}")
        payload = json.dumps(translate.from_anthropic(anth)).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    # ── 串流：逐筆 Anthropic SSE → 逐筆 OpenAI SSE，chunked 送出 ──
    def _proxy_stream(self, req):
        xlate = translate.StreamXlate()
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        with urllib.request.urlopen(req) as resp:
            for raw in resp:                       # urllib 逐行；Anthropic SSE 是行導向
                line = raw.decode("utf-8", "replace").rstrip("\r\n")
                if not line.startswith("data:"):   # event:／空行／註解都略過
                    continue
                data = line[5:].strip()
                if not data or data == "[DONE]":
                    continue
                try:
                    evt = json.loads(data)
                except ValueError:
                    continue
                for out in xlate.feed(evt):
                    self._chunk(out)
        self._chunk("data: [DONE]\n\n")  # 保底收尾（message_stop 通常已送過，重送無害）
        self._chunk("")                  # 結束 chunked

    # ── HTTP/1.1 chunked 小工具 ──
    def _chunk(self, text: str):
        data = text.encode()
        self.wfile.write(b"%X\r\n%s\r\n" % (len(data), data))
        self.wfile.flush()

    def _error(self, status: int, message: str):
        payload = json.dumps({"error": {"message": message}}).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _upstream_error(self, e: urllib.error.HTTPError):
        try:
            anth_err = json.loads(e.read() or b"{}")
            oai_err = translate.err_to_openai(anth_err)
        except ValueError:
            oai_err = {"error": {"message": "上游錯誤 HTTP %d" % e.code}}
        payload = json.dumps(oai_err).encode()
        self.send_response(e.code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def main(argv=None) -> int:
    global BASE_URL
    p = argparse.ArgumentParser(prog="anthropic-proxy", description=__doc__.splitlines()[0])
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=int(os.environ.get("PORT", 8787)))
    p.add_argument("--base", default=BASE_URL, help="上游 base URL（預設 %s）" % BASE_URL)
    args = p.parse_args(argv)

    BASE_URL = args.base
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print("anthropic-proxy 監聽 http://%s:%d → %s（version %s）" %
          (args.host, args.port, BASE_URL, API_VERSION), file=sys.stderr)
    print("cllm config：endpoint=http://%s:%d/v1/chat/completions、"
          "model=<anthropic 模型 id>、api_key=<sk-ant-...>" %
          (args.host, args.port), file=sys.stderr)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n收工。", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
