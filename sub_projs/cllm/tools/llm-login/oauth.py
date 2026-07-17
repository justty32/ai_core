"""oauth.py — OAuth 2.0 授權碼＋PKCE 協定層（供應商中立、零外部相依）。

只做「協定」：產 PKCE、組 authorize URL、起本機 callback 接授權碼、拿碼換 token、
refresh。所有供應商差異（authorize_url／token_url／client_id／scopes…）都由外部
provider dict 餵進來，本檔不硬編任何一家。純標準庫（urllib／http.server／hashlib…）。
"""

from __future__ import annotations

import base64
import hashlib
import http.server
import json
import secrets
import threading
import urllib.parse
import urllib.request


# ── PKCE（RFC 7636）───────────────────────────────────────────────
def gen_pkce() -> tuple[str, str]:
    """回傳 (verifier, challenge)。challenge = base64url(sha256(verifier))，無填充。"""
    verifier = secrets.token_urlsafe(64)  # >43 字元，符合 RFC 長度下限
    digest = hashlib.sha256(verifier.encode("ascii")).digest()
    challenge = base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii")
    return verifier, challenge


# ── 組 authorize URL ─────────────────────────────────────────────
def build_authorize_url(provider: dict, redirect_uri: str,
                        state: str, challenge: str) -> str:
    """把 authorize 端點＋標準參數＋供應商自訂 extra_auth_params 組成完整 URL。"""
    params = {
        "response_type": "code",
        "client_id": provider["client_id"],
        "redirect_uri": redirect_uri,
        "state": state,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
    }
    scopes = provider.get("scopes")
    if scopes:
        params["scope"] = " ".join(scopes) if isinstance(scopes, list) else scopes
    params.update(provider.get("extra_auth_params", {}))
    return provider["authorize_url"] + "?" + urllib.parse.urlencode(params)


# ── 本機 callback：接一次授權碼就關 ───────────────────────────────
class _Catcher(http.server.BaseHTTPRequestHandler):
    result: dict = {}
    done = threading.Event()

    def do_GET(self):  # noqa: N802（http.server 介面命名）
        q = urllib.parse.urlparse(self.path).query
        _Catcher.result = dict(urllib.parse.parse_qsl(q))
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        msg = "登入完成，可以關掉這個分頁回到終端機。" if "code" in _Catcher.result \
            else "授權失敗：" + _Catcher.result.get("error", "未知錯誤")
        self.wfile.write(("<html><body><h3>%s</h3></body></html>" % msg).encode("utf-8"))
        _Catcher.done.set()

    def log_message(self, *_):  # 靜音：不要把每個 request 印到 stderr
        pass


def capture_code(host: str, port: int, expect_state: str, timeout_s: int = 300) -> str:
    """阻塞直到 callback 進來；驗 state、回授權碼。state 不符或帶 error 即拋。"""
    _Catcher.result = {}
    _Catcher.done.clear()
    srv = http.server.HTTPServer((host, port), _Catcher)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        if not _Catcher.done.wait(timeout_s):
            raise TimeoutError("等 callback 逾時（%ds）——授權沒完成。" % timeout_s)
    finally:
        srv.shutdown()
        srv.server_close()
    res = _Catcher.result
    if "error" in res:
        raise RuntimeError("授權端回錯：%s（%s）"
                          % (res.get("error"), res.get("error_description", "")))
    if res.get("state") != expect_state:
        raise RuntimeError("state 不符——可能遭 CSRF，中止。")
    if "code" not in res:
        raise RuntimeError("callback 沒帶授權碼。")
    return res["code"]


# ── token 端點：換碼／refresh ─────────────────────────────────────
def _post_token(provider: dict, form: dict) -> dict:
    """POST application/x-www-form-urlencoded 到 token_url，回解析後的 JSON dict。"""
    secret = provider.get("client_secret")
    if secret:
        form["client_secret"] = secret
    data = urllib.parse.urlencode(form).encode("ascii")
    req = urllib.request.Request(provider["token_url"], data=data, method="POST")
    req.add_header("Content-Type", "application/x-www-form-urlencoded")
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=provider.get("timeout_s", 60)) as resp:
        return json.loads(resp.read().decode("utf-8"))


def exchange_code(provider: dict, code: str, redirect_uri: str, verifier: str) -> dict:
    """授權碼 → token 集（access／refresh／expires_in…）。"""
    return _post_token(provider, {
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": redirect_uri,
        "client_id": provider["client_id"],
        "code_verifier": verifier,
    })


def refresh(provider: dict, refresh_token: str) -> dict:
    """refresh_token → 新的 token 集。有些供應商會輪換 refresh_token，呼叫端要存回。"""
    return _post_token(provider, {
        "grant_type": "refresh_token",
        "refresh_token": refresh_token,
        "client_id": provider["client_id"],
    })
