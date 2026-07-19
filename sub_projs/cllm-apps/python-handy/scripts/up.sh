#!/usr/bin/env bash
# up.sh —— 一鍵把整條佈線拉起來，再用「本 app 的 llme」跑（照 janet 版改成呼叫 python-handy 的工具）。
#
#   llme anthropic  ──OpenAI+Bearer──▶  anthropic-proxy(sidecar)  ──x-api-key──▶  api.anthropic.com
#   憑證：ANTHROPIC_API_KEY / llm-login token / configs 內填的 key
#
# 與 janet 版的差別：janet 版把 endpoint/key/model 當 env 餵給自寫的 janet app；
# 這裡改成「把 proxy endpoint 當一個 llme endpoint（configs/anthropic.json）用」，
# 憑證則透過 llme 的 auto-inject 慣例（設 ANTHROPIC_API_KEY，llme 自動補 --api-key）帶進去。
#
# 兩種模式（MODE 環境變數）：
#   MODE=anthropic （預設）  直連 Anthropic：起 anthropic-proxy sidecar + 用 sk-ant key。
#                           ⚠ llm-login 目前「沒有」anthropic OAuth provider，
#                             此模式的憑證是「貼 sk-ant key」，非 OAuth 帳號登入。
#   MODE=openrouter          真・OAuth 帳號登入且觸及 Claude：llm-login 走 OpenRouter OAuth
#                           換不過期 key，endpoint 直指 OpenRouter（不需 proxy；需 configs/openrouter.json）。
#
# 用：./scripts/up.sh 用一句話介紹你自己
#     ./scripts/up.sh --stream 寫一首關於轉發代理的短詩
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
  # liblogin.so 可能在同目錄（vendor 情形）；讓 loader 找得到
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
    # 憑證優先序：明給的 ANTHROPIC_API_KEY → llm-login token（若有 provider）→ configs/config.json → 空
    key="${ANTHROPIC_API_KEY:-}"
    if [ -z "$key" ]; then key="$(try_login_token)"; fi
    if [ -z "$key" ] && [ -f "$HERE/configs/config.json" ]; then
      key="$(grep -o '"api_key"[^,}]*' "$HERE/configs/config.json" | sed -E 's/.*"api_key"[^"]*"([^"]*)".*/\1/' || true)"
      [ "$key" = "sk-ant-REPLACE-ME" ] && key=""
    fi
    if [ -z "$key" ]; then
      echo "⚠ 沒有 Anthropic 憑證。llme 會撞 401（憑證問題，不是 proxy 沒起）。" >&2
      echo "    → 設 ANTHROPIC_API_KEY=sk-ant-...（llme 會自動補 --api-key）" >&2
      echo "    → 或（走 OAuth）用 MODE=openrouter" >&2
    fi
    # llme 的 auto-inject：ANTHROPIC endpoint → 找 ANTHROPIC_API_KEY，自動補 --api-key
    export ANTHROPIC_API_KEY="$key"
    echo "· 跑 app：llme anthropic（endpoint 見 configs/anthropic.json；api_key=$([ -n "$key" ] && echo 已設 || echo 未設)）"
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
    if [ ! -f "$HERE/configs/openrouter.json" ]; then
      echo "✗ 缺 configs/openrouter.json（endpoint=https://openrouter.ai/api/v1/chat/completions、model 自填）" >&2
      exit 1
    fi
    # llme 的 auto-inject：OPENROUTER endpoint → 找 OPENROUTER_API_KEY，自動補 --api-key
    export OPENROUTER_API_KEY="$key"
    echo "· OpenRouter OAuth token 就緒，endpoint 直指 OpenRouter（不需 proxy）"
    exec "$HERE/llme" openrouter "$@"
    ;;

  *)
    echo "未知 MODE：$MODE（用 anthropic 或 openrouter）" >&2; exit 2 ;;
esac
