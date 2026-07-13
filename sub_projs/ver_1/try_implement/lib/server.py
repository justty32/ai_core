#!/usr/bin/env python3
"""server — persistent server 的標準 lifecycle（NDJSON 行協議）。

對應 thinking_pending.md §3「持久性程式建議設計成 server」。把 SFC forge 那個
「啟動 → 就緒 → 逐行處理請求 → 關閉」的迴圈泛化成可重用的 library。SFC forge 是
這個模式的具體實例（forge 是「載入 tiny function 後查表執行」的 server）。

== 標準 lifecycle ==
    start    → 載入資源（caller 在建構 / on_start 做）
    ready    → 往 stderr 印一行 ready 訊號（caller 可被外部探測）
    serve    → 逐行讀 stdin 的 JSON request，dispatch 到 handler，回一行 JSON response
    shutdown → 收到 shutdown 指令或 stdin EOF，跑 on_shutdown，結束

== 對外契約（NDJSON：newline-delimited JSON）==
請求：每行一個 JSON object，含路由鍵（預設 "cmd"）指出要呼叫哪個 handler。
回應：每行一個 JSON object，server 自動補 "ok": true/false。
內建指令：ping / list（列出 handler）/ shutdown。

== 設計決策 ==
1. 介面用 stdin/stdout NDJSON：最 KISS，免 socket/port/http。thinking_pending §3 指出
   此選項需重評——保留升 HTTP 的空間（handler 簽名不變，只需換傳輸層）。標為暫定。
2. handler 簽名 ``fn(req: dict) -> dict``。回傳的 dict 會被包進 ``{"ok": true, **result}``；
   拋例外則 ``{"ok": false, "error": "..."}``。
3. serve() 接受任意 readable/writable（預設 sys.stdin/stdout），方便用 io.StringIO 測試。
"""

from __future__ import annotations

import json
import sys
from typing import Any, Callable, Iterable, TextIO

Handler = Callable[[dict], dict]


class NDJSONServer:
    """逐行 JSON 請求/回應的 persistent server。"""

    def __init__(self, name: str, route_key: str = "cmd"):
        self.name = name
        self.route_key = route_key
        self._handlers: dict[str, Handler] = {}
        self._on_start: Callable[[], None] | None = None
        self._on_shutdown: Callable[[], None] | None = None

    # ---- 註冊 ----

    def handler(self, name: str) -> Callable[[Handler], Handler]:
        """裝飾器：``@server.handler("foo")``。"""
        def deco(fn: Handler) -> Handler:
            self._handlers[name] = fn
            return fn
        return deco

    def register(self, name: str, fn: Handler) -> None:
        self._handlers[name] = fn

    def on_start(self, fn: Callable[[], None]) -> None:
        self._on_start = fn

    def on_shutdown(self, fn: Callable[[], None]) -> None:
        self._on_shutdown = fn

    # ---- lifecycle ----

    def _ready_signal(self, err: TextIO) -> None:
        print(
            f"[{self.name}] ready：{len(self._handlers)} handlers {sorted(self._handlers)}",
            file=err,
        )
        err.flush()

    def _dispatch(self, req: dict) -> tuple[dict, bool]:
        """回 (response, should_stop)。"""
        key = req.get(self.route_key)

        # 內建指令
        if key == "ping":
            return ({"ok": True, "pong": True}, False)
        if key == "list":
            return ({"ok": True, "handlers": sorted(self._handlers)}, False)
        if key == "shutdown":
            return ({"ok": True, "shutdown": True}, True)

        if key not in self._handlers:
            return ({"ok": False, "error": f"unknown {self.route_key}: {key!r}"}, False)

        try:
            result = self._handlers[key](req)
            resp = {"ok": True}
            if result:
                resp.update(result)
            return (resp, False)
        except Exception as exc:  # noqa: BLE001
            return ({"ok": False, "error": f"{type(exc).__name__}: {exc}"}, False)

    def _process_line(self, line: str) -> tuple[str | None, bool]:
        """處理一行：回 (要寫回的 JSON 字串或 None, 是否該停)。空行回 (None, False)。"""
        line = line.strip()
        if not line:
            return (None, False)
        try:
            req = json.loads(line)
        except json.JSONDecodeError as exc:
            return (json.dumps({"ok": False, "error": f"bad json: {exc}"}), False)
        resp, stop = self._dispatch(req)
        return (json.dumps(resp, ensure_ascii=False), stop)

    def serve(
        self,
        stdin: Iterable[str] | None = None,
        stdout: TextIO | None = None,
        stderr: TextIO | None = None,
    ) -> int:
        """跑完整 lifecycle（stdin/stdout 傳輸）。stdin 為可迭代的行來源（預設 sys.stdin）。"""
        stdin = sys.stdin if stdin is None else stdin
        out = sys.stdout if stdout is None else stdout
        err = sys.stderr if stderr is None else stderr

        if self._on_start:
            self._on_start()
        self._ready_signal(err)

        for line in stdin:
            payload, stop = self._process_line(line)
            if payload is not None:
                out.write(payload + "\n")
                out.flush()
            if stop:
                break

        if self._on_shutdown:
            self._on_shutdown()
        print(f"[{self.name}] shutdown", file=err)
        return 0

    def serve_socket(self, socket_path: str, stderr: TextIO | None = None) -> int:
        """跑完整 lifecycle（Unix domain socket 傳輸）——**長駐單例**版。

        與 serve() 的關鍵差別：多個獨立的 one-shot caller 可**連上同一個 server 實例**，
        共用同一份 handler 閉包狀態（如 LLM Entry Manager 的 RateMeter）→ consume rate
        能**跨呼叫累計**。這正是 stdin/stdout 傳輸做不到、而 README「Gap G」要解的點。

        語意上仍是 singleton（一次處理一個 caller）：serial accept，handler 完一個連線
        才接下一個，不開 thread。單一連線內可送多行請求；連線 EOF 只結束**該連線**，
        唯有收到 `shutdown` 指令才停掉整個 server（並清掉 socket 檔）。
        Unix socket 純標準庫、POSIX 原生、免 port 管理（Windows 不在考慮範圍）。
        """
        import os
        import socket as _socket

        err = sys.stderr if stderr is None else stderr
        if os.path.exists(socket_path):
            os.unlink(socket_path)  # 清掉殘留的 stale socket

        srv = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
        srv.bind(socket_path)
        srv.listen(1)  # singleton：一次一個

        if self._on_start:
            self._on_start()
        print(
            f"[{self.name}] ready（unix socket {socket_path}）："
            f"{len(self._handlers)} handlers {sorted(self._handlers)}",
            file=err,
        )
        err.flush()

        stop_all = False
        try:
            while not stop_all:
                conn, _ = srv.accept()
                with conn:
                    rf = conn.makefile("r", encoding="utf-8")
                    wf = conn.makefile("w", encoding="utf-8")
                    for line in rf:
                        payload, stop = self._process_line(line)
                        if payload is not None:
                            wf.write(payload + "\n")
                            wf.flush()
                        if stop:
                            stop_all = True
                            break
        finally:
            srv.close()
            if os.path.exists(socket_path):
                os.unlink(socket_path)
            if self._on_shutdown:
                self._on_shutdown()
            print(f"[{self.name}] shutdown", file=err)
        return 0
