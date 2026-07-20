"""http.py — 傳輸管子：統一 file:// 與真 HTTP，回 (status, chunk 迭代器)。
對齊 core/src/http.cpp（do_file 路徑正規化＋run 傳輸）。

⚠ 陷阱備忘：file://<絕對路徑> 特例照 http.cpp 的 do_file 規格：跳過 "file://" 後，若開頭是
"/X:" 這種（Windows 的 file:///C:/…）去掉那個前導 '/'；POSIX 的 "/home/…" 保留。Windows +
POSIX 雙可跑，好讓 CLI 拿 test/fixtures 的假回應離線自測。
"""
import urllib.error
import urllib.request

from .errors import LLMError


# ── file:// 路徑正規化（逐字對齊 core/src/http.cpp 的 do_file）──
def _file_url_to_path(url):
    """把 file:// URL 轉成本機路徑。Windows(file:///C:/…) 去前導 '/'；POSIX(/home/…) 保留。"""
    path = url[7:]  # 跳過 "file://"
    if len(path) >= 3 and path[0] == "/" and path[2] == ":":
        path = path[1:]
    return path


# ── 傳輸（統一 file:// 與真 HTTP，回 (status, chunk 迭代器)；對齊 http.cpp 的 run）──
def _transport(url, headers, body, timeout_ms):
    if url.startswith("file://"):
        path = _file_url_to_path(url)

        def gen_file():
            try:
                f = open(path, "rb")
            except OSError as e:
                raise LLMError("http: 開不了 file:// 路徑：" + path) from e
            with f:
                while True:
                    b = f.read(8192)
                    if not b:
                        break
                    yield b
        return 200, gen_file()

    timeout = (timeout_ms / 1000.0) if timeout_ms and timeout_ms > 0 else None
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        resp = urllib.request.urlopen(req, timeout=timeout)  # noqa: S310（url 由呼叫端控制）
    except urllib.error.HTTPError as e:  # 4xx/5xx：讀 body 交給 guard 處理錯誤語意
        data = e.read()
        return e.code, iter((data,))
    except urllib.error.URLError as e:  # 連不上／逾時：對齊 C++ 的「傳輸失敗」
        raise LLMError("http: 傳輸失敗——" + str(e.reason)) from e
    except OSError as e:
        raise LLMError("http: 傳輸失敗——" + str(e)) from e

    def gen_http():
        with resp:
            while True:
                b = resp.read(8192)
                if not b:
                    break
                yield b
    return (resp.getcode() or 200), gen_http()
