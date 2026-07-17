"""store.py — llm-login 的狀態層：provider 設定、token 存放、回頭餵 cllm config。

三個檔（都在 ~/.config/llm/，可用環境變數改）：
  oauth.json         provider 設定（authorize_url／token_url／client_id／scopes…），使用者手填
  oauth_token.json   token 存放（access／refresh／expires_at），本工具寫、0600
  config.json        cllm CLI 自己的 config——本工具只 patch 其中的 api_key，其餘鍵原樣保留

token 是機密：寫檔一律 0600。config.json 走「讀→改一鍵→寫回」，不碰使用者其他設定，
也不塞 cllm（glaze 嚴格解析）不認得的鍵。
"""

from __future__ import annotations

import json
import os
import time


def config_dir() -> str:
    base = os.environ.get("LLM_CONFIG_DIR")
    if base:
        return base
    home = os.environ.get("HOME", "")
    return os.path.join(home, ".config", "llm")


def _path(name: str) -> str:
    return os.path.join(config_dir(), name)


def provider_path() -> str:
    return os.environ.get("LLM_OAUTH_PROVIDER") or _path("oauth.json")


def token_path() -> str:
    return _path("oauth_token.json")


def cllm_config_path() -> str:
    # 與 cli_config.cpp 對齊：LLM_CLI_CONFIG ＞ 家目錄 ~/.config/llm/config.json
    return os.environ.get("LLM_CLI_CONFIG") or _path("config.json")


def _write_secret(path: str, obj: dict) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    # 先建 0600 再寫，避免明文短暫以寬鬆權限落地
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)


# ── provider 設定 ────────────────────────────────────────────────
def load_provider() -> dict:
    p = provider_path()
    if not os.path.exists(p):
        raise FileNotFoundError(
            "找不到 provider 設定：%s\n"
            "先照 oauth.example.json 複製一份填好（authorize_url／token_url／"
            "client_id／redirect_port／scopes）。" % p)
    with open(p, encoding="utf-8") as f:
        prov = json.load(f)
    for key in ("authorize_url", "token_url", "client_id"):
        if not prov.get(key):
            raise ValueError("provider 設定缺必填欄位：%s（%s）" % (key, p))
    return prov


# ── token 存放 ───────────────────────────────────────────────────
def save_token(tok: dict) -> dict:
    """把 token 端點回應正規化成 {access_token, refresh_token, expires_at, token_type}
    後 0600 落檔；輪換來的 refresh_token 若缺就沿用舊的。回傳存下的記錄。"""
    prev = load_token_silent()
    rec = {
        "access_token": tok["access_token"],
        "token_type": tok.get("token_type", "Bearer"),
        "refresh_token": tok.get("refresh_token") or prev.get("refresh_token"),
        "scope": tok.get("scope", prev.get("scope")),
    }
    if "expires_in" in tok:
        rec["expires_at"] = int(time.time()) + int(tok["expires_in"])
    _write_secret(token_path(), rec)
    return rec


def load_token_silent() -> dict:
    p = token_path()
    if not os.path.exists(p):
        return {}
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def is_expired(rec: dict, skew_s: int = 60) -> bool:
    """離過期不到 skew_s 就當作要換。無 expires_at（不過期型）→ 永不過期。"""
    exp = rec.get("expires_at")
    if exp is None:
        return False
    return time.time() >= exp - skew_s


# ── 回頭餵 cllm config：只 patch api_key ─────────────────────────
def patch_cllm_api_key(access_token: str) -> str:
    p = cllm_config_path()
    cfg = {}
    if os.path.exists(p):
        with open(p, encoding="utf-8") as f:
            cfg = json.load(f)
    cfg["api_key"] = access_token
    _write_secret(p, cfg)  # 含 bearer，一律 0600
    return p
