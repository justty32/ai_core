#!/usr/bin/env python3
"""llm-login — 用「帳號登入（OAuth 授權碼＋PKCE）」換 token 餵給 cllm 的 llm CLI。

cllm 本體不懂「登入」：它只把 config 的 api_key 塞進 Authorization: Bearer。本工具補上
缺的那一段——做 OAuth 登入拿到 token、到期自動 refresh、把當前有效 access_token 寫進
cllm 的 config.json。cllm 的 C/C++ 核心一行不動。

    login     瀏覽器登入一次，拿 token 並寫進 cllm config
    refresh   用 refresh_token 換新的 access_token，更新 cllm config
    token     印出當前有效 access_token（快過期會自動 refresh）——給腳本用：
                  llm 你好 --api-key "$(llm_login.py token)"
    status    顯示目前 token 狀態（不外洩 token 本身）

供應商設定放 ~/.config/llm/oauth.json（見 oauth.example.json）。本工具供應商中立，
不預設任何一家。⚠ 只對「有提供 OAuth 給程式化／API 存取」的供應商合法適用；拿它去套
消費級網頁訂閱的 session 屬濫用，別這樣用（見 README）。
"""

from __future__ import annotations

import argparse
import secrets
import sys
import time
import webbrowser

import oauth
import store


def _redirect_uri(prov: dict) -> tuple[str, str, int]:
    host = prov.get("redirect_host", "127.0.0.1")
    port = int(prov.get("redirect_port", 8123))
    path = prov.get("redirect_path", "/callback")
    return "http://%s:%d%s" % (host, port, path), host, port


def _open(url: str) -> None:
    print("開瀏覽器登入；若沒自動彈出，手動開這個網址：\n  " + url, file=sys.stderr)
    webbrowser.open(url)


def cmd_login(_args) -> int:
    prov = store.load_provider()
    redirect_uri, host, port = _redirect_uri(prov)
    verifier, challenge = oauth.gen_pkce()

    if prov.get("flow") == "openrouter":
        # 非標準：callback_url＋PKCE、無 state、JSON 交換回「不過期 API key」
        url = oauth.build_authorize_url_openrouter(prov, redirect_uri, challenge)
        _open(url)
        code = oauth.capture_code(host, port, None)
        raw = oauth.exchange_code_openrouter(prov, code, verifier)
        key = raw.get("key") or raw.get("access_token")
        if not key:
            raise RuntimeError("交換回應沒有 key/access_token：%s" % raw)
        tok = {"access_token": key}          # 無 expires_in→不過期、無 refresh
    else:
        state = secrets.token_urlsafe(16)
        url = oauth.build_authorize_url(prov, redirect_uri, state, challenge)
        _open(url)
        code = oauth.capture_code(host, port, state)
        tok = oauth.exchange_code(prov, code, redirect_uri, verifier)

    rec = store.save_token(tok)
    path = store.patch_cllm(rec["access_token"], prov.get("cllm_endpoint"),
                           prov.get("cllm_model"))
    print("登入成功，憑證已寫進 %s。%s" % (path, _expiry_note(rec)), file=sys.stderr)
    return 0


def cmd_refresh(_args) -> int:
    prov = store.load_provider()
    rec = store.load_token_silent()
    if not rec.get("refresh_token"):
        if rec and rec.get("expires_at") is None:
            print("此憑證不過期（如 OpenRouter API key）——無需 refresh。", file=sys.stderr)
            return 0
        print("沒有 refresh_token——重跑 login。", file=sys.stderr)
        return 1
    rec = store.save_token(oauth.refresh(prov, rec["refresh_token"]))
    path = store.patch_cllm(rec["access_token"], prov.get("cllm_endpoint"),
                           prov.get("cllm_model"))
    print("已 refresh，憑證更新到 %s。%s" % (path, _expiry_note(rec)), file=sys.stderr)
    return 0


def cmd_token(_args) -> int:
    """印出有效憑證；快過期就先自動 refresh。stdout 只有憑證，方便 $(...)。"""
    rec = store.load_token_silent()
    if not rec:
        print("還沒登入——先跑 login。", file=sys.stderr)
        return 1
    if store.is_expired(rec):
        prov = store.load_provider()
        if not rec.get("refresh_token"):
            print("憑證過期且無 refresh_token——重跑 login。", file=sys.stderr)
            return 1
        rec = store.save_token(oauth.refresh(prov, rec["refresh_token"]))
        store.patch_cllm(rec["access_token"], prov.get("cllm_endpoint"),
                        prov.get("cllm_model"))
    print(rec["access_token"])
    return 0


def cmd_status(_args) -> int:
    rec = store.load_token_silent()
    if not rec:
        print("狀態：未登入。", file=sys.stderr)
        return 1
    tail = rec["access_token"][-6:] if rec.get("access_token") else "??????"
    print("狀態：已登入（access_token …%s）。%s%s" % (
        tail, _expiry_note(rec),
        "" if rec.get("refresh_token") else "（無 refresh_token）"), file=sys.stderr)
    return 0


def _expiry_note(rec: dict) -> str:
    exp = rec.get("expires_at")
    if exp is None:
        return "不過期型 token。"
    left = int(exp - time.time())
    return "剩約 %d 分鐘到期。" % (left // 60) if left > 0 else "已過期，下次用會自動 refresh。"


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="llm-login", description=__doc__.splitlines()[0])
    sub = p.add_subparsers(dest="cmd", required=True)
    for name, fn in (("login", cmd_login), ("refresh", cmd_refresh),
                     ("token", cmd_token), ("status", cmd_status)):
        sp = sub.add_parser(name)
        sp.set_defaults(fn=fn)
    args = p.parse_args(argv)
    try:
        return args.fn(args)
    except Exception as e:  # 對使用者印一行人話，不吐整串 traceback
        print("錯誤：%s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
