#!/usr/bin/env bash
# up.sh —— 一鍵把整條佈線拉起來，再用本 app 的 bin/llme 打（照 janet-try-1/scripts/up.sh）。
#
#   bin/llme anthropic … ──OpenAI+Bearer──▶ anthropic-proxy(sidecar) ──x-api-key──▶ api.anthropic.com
#   憑證：ANTHROPIC_API_KEY ＞ llm-login token ＞ configs/anthropic.json 的 api_key
#
# 兩種模式（MODE 環境變數）：
#   MODE=anthropic （預設）  起 anthropic-proxy sidecar（TCP health-check）＋備憑證，
#                           然後 exec bin/llme anthropic（把 proxy endpoint 當一個 llme endpoint）。
#                           ⚠ llm-login 目前沒有 anthropic OAuth provider，本模式憑證＝貼 sk-ant key。
#   MODE=openrouter          走 OpenRouter OAuth 換不過期 key，endpoint 直指 OpenRouter（不需 proxy）。
#                           需 configs/openrouter.json（OpenRouter endpoint）。
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

# 確保 app 已編（沒編就 build）
[ -x "$HERE/bin/llme" ] || "$HERE/build.sh"

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
    # 憑證優先序：明給的 ANTHROPIC_API_KEY → llm-login token → configs/anthropic.json → 空
    key="${ANTHROPIC_API_KEY:-}"
    if [ -z "$key" ]; then key="$(try_login_token)"; fi
    if [ -z "$key" ] && [ -f "$HERE/configs/anthropic.json" ]; then
      key="$(grep -o '"api_key"[^,}]*' "$HERE/configs/anthropic.json" | sed -E 's/.*"api_key"[^"]*"([^"]*)".*/\1/' || true)"
      [ "$key" = "sk-ant-REPLACE-ME" ] && key=""
    fi
    if [ -z "$key" ]; then
      echo "⚠ 沒有 Anthropic 憑證。app 會撞 401（憑證問題，不是 proxy 沒起）。" >&2
      echo "    → 設 ANTHROPIC_API_KEY=sk-ant-...；或（走 OAuth）用 MODE=openrouter" >&2
    fi
    # llme 的自動注入慣例：endpoint「anthropic」→ 找 ANTHROPIC_API_KEY，補 --api-key。
    export ANTHROPIC_API_KEY="$key"
    echo "· 跑 app：bin/llme anthropic（proxy 127.0.0.1:$PORT，api_key=$([ -n "$key" ] && echo 已設 || echo 未設)）"
    exec "$HERE/bin/llme" anthropic "$@"
    ;;

  openrouter)
    key="$(try_login_token)"
    if [ -z "$key" ]; then
      echo "✗ OpenRouter OAuth 尚無有效 token。先做帳號登入（開瀏覽器一次）：" >&2
      echo "    cp $CLLM_ROOT/tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json" >&2
      echo "    $(find_bin llm-login) login        # 帳號登入 → 換不過期 key" >&2
      exit 1
    fi
    if [ ! -f "$HERE/configs/openrouter.json" ]; then
      echo "✗ 缺 configs/openrouter.json（OpenRouter endpoint config）。" >&2
      echo "    範例：{ \"endpoint\": \"https://openrouter.ai/api/v1/chat/completions\", \"model\": \"anthropic/claude-3.5-sonnet\" }" >&2
      exit 1
    fi
    # llme 的自動注入慣例：endpoint「openrouter」→ 找 OPENROUTER_API_KEY，補 --api-key。
    export OPENROUTER_API_KEY="$key"
    echo "· OpenRouter OAuth token 就緒，endpoint 直指 OpenRouter（不需 proxy）"
    exec "$HERE/bin/llme" openrouter "$@"
    ;;

  *)
    echo "未知 MODE：$MODE（用 anthropic 或 openrouter）" >&2; exit 2 ;;
esac
