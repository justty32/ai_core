#!/usr/bin/env python3
"""test_offline.py — llm-login 離線煙霧測試（不連網、不開瀏覽器）。

驗可離線驗的部分：PKCE challenge 正確性、authorize URL 組裝、token store 正規化＋
round-trip、is_expired 邊界、patch cllm config 只動 api_key 不傷其他鍵。真 OAuth 往返
（login/refresh 打真 endpoint）本質要真供應商，屬 WAIT_USER。
"""

from __future__ import annotations

import base64
import hashlib
import json
import os
import tempfile
import urllib.parse

import oauth
import store


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)
    print("  ok  " + msg)


def test_pkce():
    v, c = oauth.gen_pkce()
    want = base64.urlsafe_b64encode(hashlib.sha256(v.encode()).digest()).rstrip(b"=").decode()
    check(c == want, "PKCE challenge = base64url(sha256(verifier)) 無填充")
    check("=" not in c and len(v) >= 43, "verifier 長度≥43、challenge 無 padding")


def test_authorize_url():
    prov = {"authorize_url": "https://p.example/auth", "client_id": "cid",
            "scopes": ["a", "b"], "extra_auth_params": {"prompt": "login"}}
    url = oauth.build_authorize_url(prov, "http://127.0.0.1:8123/callback", "st8", "chal")
    q = dict(urllib.parse.parse_qsl(urllib.parse.urlparse(url).query))
    check(q["code_challenge_method"] == "S256", "帶 S256")
    check(q["scope"] == "a b", "scopes 以空白併接")
    check(q["state"] == "st8" and q["code_challenge"] == "chal", "state/challenge 帶上")
    check(q["prompt"] == "login", "extra_auth_params 併入")


def test_authorize_url_openrouter():
    prov = {"authorize_url": "https://openrouter.ai/auth"}
    url = oauth.build_authorize_url_openrouter(prov, "http://127.0.0.1:8123/callback", "chal")
    q = dict(urllib.parse.parse_qsl(urllib.parse.urlparse(url).query))
    check(q["callback_url"] == "http://127.0.0.1:8123/callback", "OpenRouter 用 callback_url")
    check(q["code_challenge_method"] == "S256" and q["code_challenge"] == "chal", "帶 PKCE")
    check("client_id" not in q and "state" not in q, "OpenRouter 免 client_id／無 state")


def test_token_store_roundtrip():
    with tempfile.TemporaryDirectory() as d:
        os.environ["LLM_CONFIG_DIR"] = d
        rec = store.save_token({"access_token": "AT1", "expires_in": 3600,
                                "refresh_token": "RT1", "token_type": "Bearer"})
        check(rec["refresh_token"] == "RT1" and rec["expires_at"] > 0, "首存帶 refresh＋算 expires_at")
        # 第二次回應沒帶 refresh_token → 沿用舊的
        rec2 = store.save_token({"access_token": "AT2", "expires_in": 3600})
        check(rec2["refresh_token"] == "RT1", "缺 refresh_token 時沿用舊的（不弄丟續期能力）")
        check(store.load_token_silent()["access_token"] == "AT2", "落檔 round-trip")
        mode = oct(os.stat(store.token_path()).st_mode)[-3:]
        check(mode == "600", "token 檔權限 0600")


def test_is_expired():
    check(store.is_expired({"expires_at": 0}) is True, "過去時間＝過期")
    check(store.is_expired({}) is False, "無 expires_at＝不過期型")
    import time as _t
    check(store.is_expired({"expires_at": _t.time() + 3600}) is False, "一小時後＝未過期")


def test_patch_config_preserves_keys():
    with tempfile.TemporaryDirectory() as d:
        os.environ["LLM_CONFIG_DIR"] = d
        cfg_path = store.cllm_config_path()
        os.makedirs(d, exist_ok=True)
        with open(cfg_path, "w", encoding="utf-8") as f:
            json.dump({"endpoint": "http://x/v1/chat/completions", "model": "m",
                       "timeout_ms": 120000}, f)
        store.patch_cllm("BEARER123")  # 不帶 endpoint/model
        got = json.load(open(cfg_path, encoding="utf-8"))
        check(got["api_key"] == "BEARER123", "api_key 寫入")
        check(got["endpoint"] == "http://x/v1/chat/completions" and got["model"] == "m"
              and got["timeout_ms"] == 120000, "沒帶就不動 endpoint/model、其他鍵原樣保留")
        # preset 帶 endpoint/model 時一併設
        store.patch_cllm("BEARER456", "https://y/v1/chat/completions", "gemini-2.5-flash")
        got = json.load(open(cfg_path, encoding="utf-8"))
        check(got["endpoint"] == "https://y/v1/chat/completions"
              and got["model"] == "gemini-2.5-flash" and got["timeout_ms"] == 120000,
              "帶了就一步設好 endpoint/model，timeout_ms 仍保留")


def test_presets_load():
    here = os.path.dirname(os.path.abspath(__file__))
    pdir = os.path.join(here, "providers")
    names = sorted(f for f in os.listdir(pdir) if f.endswith(".json"))
    check(len(names) >= 4, "至少四個 preset：%s" % names)
    for n in names:
        os.environ["LLM_OAUTH_PROVIDER"] = os.path.join(pdir, n)
        prov = store.load_provider()  # JSON 合法＋必填欄位齊（openrouter 免 client_id）
        check(bool(prov.get("cllm_endpoint")), "%s 帶 cllm_endpoint" % n)
    del os.environ["LLM_OAUTH_PROVIDER"]


def main():
    for t in (test_pkce, test_authorize_url, test_authorize_url_openrouter,
              test_token_store_roundtrip, test_is_expired,
              test_patch_config_preserves_keys, test_presets_load):
        print(t.__name__)
        t()
    print("\n全部離線煙霧測試通過。")


if __name__ == "__main__":
    main()
