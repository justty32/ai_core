#!/usr/bin/env bash
# up.sh —— 一鍵把整條佈線拉起來，再用「本 app 的 llme」跑（INTEGRATION.md「情境 B / up 動作」）。
#
#   llme anthropic ──OpenAI+Bearer──▶ anthropic-proxy(sidecar) ──x-api-key──▶ api.anthropic.com
#   憑證：llm-login（OAuth 帳號登入，若有 provider）或直接填 sk-ant key
#
# 與 janet/lisp-try-1 版的差別：那些跑一支專用 app（bin/main.janet / lisp binding）；
# 本 app 沒有專用 binding——它用 llme（薄轉發器）＋configs/<endpoint>.json 打後端。
# 所以 up.sh 備好 proxy + 憑證後，直接 `exec llme anthropic "$@"`：把 proxy endpoint
# 當成 llme 的一個 endpoint（configs/anthropic.json 已指向 127.0.0.1:8787），
# 憑證經 ANTHROPIC_API_KEY 交給 llme 自動注入 --api-key。
#
# 兩種模式（MODE 環境變數）：
#   MODE=anthropic （預設）  直連 Anthropic：起 anthropic-proxy sidecar + 用 sk-ant key。
#   MODE=openrouter          真・OAuth 帳號登入且觸及 Claude：llm-login 走 OpenRouter OAuth
#                           換不過期 key，endpoint 直指 OpenRouter（OpenAI-compat，不需 proxy）。
#
# 用：./scripts/up.sh --stream 用一句話介紹你自己
#     MODE=openrouter ./scripts/up.sh 你好
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${MODE:-anthropic}"
PORT="${PORT:-8787}"
CLLM_ROOT="${CLLM_ROOT:-$HOME/repo/ai_core/sub_projs/cllm}"
RUNDIR="$HERE/.run"; mkdir -p "$RUNDIR"
PIDFILE="$RUNDIR/proxy.pid"; LOGFILE="$RUNDIR/proxy.log"

# ── 定位工具二進位：vendor/ 優先 → build 產物 → PATH ───────────────
find_bin() {
  local name="$1"
  if [ -x "$HERE/vendor/cllm-tools/$name" ]; then echo "$HERE/vendor/cllm-tools/$name"; return; fi
  if [ -x "$CLLM_ROOT/build/tools/$name" ];   then echo "$CLLM_ROOT/build/tools/$name";   return; fi
  command -v "$name" 2>/dev/null || true
}

tcp_up() { (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; }   # port 通 = sidecar 在

start_proxy() {
  local bin; bin="$(find_bin anthropic-proxy)"
  if [ -z "$bin" ]; then
    echo "✗ 找不到 anthropic-proxy —— 先 build 或 vendor：" >&2
    echo "    cd $CLLM_ROOT && cmake --build --preset linux-debug --target anthropic-proxy" >&2
    echo "    或 ./scripts/vendor.sh" >&2
    exit 1
  fi
  if tcp_up; then echo "· proxy 已在 127.0.0.1:$PORT"; return; fi
  echo "· 起 proxy：$bin（127.0.0.1:$PORT）"
  LD_LIBRARY_PATH="$(dirname "$bin"):${LD_LIBRARY_PATH:-}" \
    nohup "$bin" --host 127.0.0.1 --port "$PORT" >>"$LOGFILE" 2>&1 &
  echo $! >"$PIDFILE"
  for _ in 1 2 3 4 5 6 7 8 9 10; do tcp_up && break; sleep 0.3; done
  if ! tcp_up; then
    echo "✗ proxy 起不來，看日誌：$LOGFILE" >&2; tail -n 20 "$LOGFILE" >&2 || true; exit 3
  fi
  echo "· proxy health-check 通過（connection refused 這類就不會發生了）"
}

# 嘗試用 llm-login 拿 OAuth token（provider 已配置時）；回印 token 或空字串
try_login_token() {
  local bin; bin="$(find_bin llm-login)"
  [ -z "$bin" ] && return 0
  LD_LIBRARY_PATH="$(dirname "$bin"):${LD_LIBRARY_PATH:-}" "$bin" token 2>/dev/null || true
}

case "$MODE" in
  anthropic)
    start_proxy
    # 憑證優先序：明給的 ANTHROPIC_API_KEY → llm-login token（若有 provider）→ 空
    key="${ANTHROPIC_API_KEY:-}"
    if [ -z "$key" ]; then key="$(try_login_token)"; fi
    if [ -z "$key" ]; then
      echo "⚠ 沒有 Anthropic 憑證。llm 會撞 401（憑證問題，不是 proxy 沒起）。" >&2
      echo "    → 設 ANTHROPIC_API_KEY=sk-ant-...；或（走 OAuth）用 MODE=openrouter" >&2
    fi
    echo "· 跑：llme anthropic $* （endpoint=configs/anthropic.json → 127.0.0.1:$PORT）"
    # llme 會從 ANTHROPIC_API_KEY 自動注入 --api-key（anthropic → ANTHROPIC_API_KEY）
    export ANTHROPIC_API_KEY="$key"
    exec "$HERE/llme" anthropic "$@"
    ;;

  openrouter)
    key="$(try_login_token)"
    if [ -z "$key" ]; then
      echo "✗ OpenRouter OAuth 尚無有效 token。先做帳號登入（開瀏覽器一次）：" >&2
      echo "    cp $CLLM_ROOT/tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json" >&2
      echo "    $(find_bin llm-login) login        # 帳號登入 → 換不過期 key" >&2
      exit 1
    fi
    # llme 是 config-based，openrouter 不在 configs/ 內：即時生一份到 .run/ 並用 LLME_CONFIG_DIR 指過去。
    ORDIR="$RUNDIR/or-config"; mkdir -p "$ORDIR"
    cat >"$ORDIR/openrouter.json" <<JSON
{
  "endpoint": "https://openrouter.ai/api/v1/chat/completions",
  "model": "${APP_MODEL:-anthropic/claude-3.5-sonnet}",
  "timeout_ms": 120000
}
JSON
    echo "· OpenRouter OAuth token 就緒，endpoint 直指 OpenRouter（不需 proxy）"
    echo "· 跑：llme openrouter $*"
    export OPENROUTER_API_KEY="$key"
    exec env LLME_CONFIG_DIR="$ORDIR" "$HERE/llme" openrouter "$@"
    ;;

  *)
    echo "未知 MODE：$MODE（用 anthropic 或 openrouter）" >&2; exit 2 ;;
esac
