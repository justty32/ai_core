#!/usr/bin/env bash
# proxyctl.sh —— 免 systemd 的一鍵啟停：背景跑 anthropic-proxy、記 PID、看日誌。
#
# 給沒有／不想用 systemd 的情境（或想一行搞定）。systemd 用戶請優先用
# service/anthropic-proxy.service（更省心、能開機自啟）。
#
#   ./proxyctl.sh start     # nohup 背景起，PID＋日誌落在 XDG runtime／state 目錄
#   ./proxyctl.sh stop      # 停
#   ./proxyctl.sh restart
#   ./proxyctl.sh status    # 在跑嗎？PID／埠／日誌路徑
#   ./proxyctl.sh logs      # tail -f 日誌
#
# 環境變數（可選）：PORT（預設 8787）、ANTHROPIC_BASE_URL、ANTHROPIC_VERSION、ANTHROPIC_MAX_TOKENS。
# 本腳本『不會被別人自動呼叫』——要跑代理，你自己下 start。

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # …/anthropic-proxy
PORT="${PORT:-8787}"
RUNDIR="${XDG_RUNTIME_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}}/anthropic-proxy"
PIDFILE="$RUNDIR/proxy.pid"
LOGFILE="$RUNDIR/proxy.log"

mkdir -p "$RUNDIR"

_alive() { [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; }

start() {
  if _alive; then echo "已在跑（PID $(cat "$PIDFILE")，埠 $PORT）。"; return 0; fi
  cd "$HERE"
  nohup python3 proxy.py --host 127.0.0.1 --port "$PORT" >>"$LOGFILE" 2>&1 &
  echo $! >"$PIDFILE"
  sleep 0.5
  if _alive; then
    echo "已起：PID $(cat "$PIDFILE")，http://127.0.0.1:$PORT/v1/chat/completions"
    echo "日誌：$LOGFILE"
  else
    echo "起不來，看日誌：$LOGFILE" >&2; tail -n 20 "$LOGFILE" >&2 || true; return 1
  fi
}

stop() {
  if _alive; then
    kill "$(cat "$PIDFILE")" && echo "已停（PID $(cat "$PIDFILE")）。"
  else
    echo "沒在跑。"
  fi
  rm -f "$PIDFILE"
}

status() {
  if _alive; then
    echo "狀態：在跑。PID $(cat "$PIDFILE")，埠 $PORT。"
    echo "日誌：$LOGFILE"
  else
    echo "狀態：沒在跑。"
    return 1
  fi
}

case "${1:-}" in
  start)   start ;;
  stop)    stop ;;
  restart) stop; sleep 0.3; start ;;
  status)  status ;;
  logs)    touch "$LOGFILE"; tail -f "$LOGFILE" ;;
  *) echo "用法：$0 {start|stop|restart|status|logs}" >&2; exit 2 ;;
esac
