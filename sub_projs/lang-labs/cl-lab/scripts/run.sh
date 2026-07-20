#!/usr/bin/env bash
# cl-lab 開發用小工具。用法：
#   scripts/run.sh run [args...]   直接跑 CLI（開發主循環）
#   scripts/run.sh test            跑測試
#   scripts/run.sh build           編出獨立執行檔 build/cl-lab
#   scripts/run.sh repl            開一個已載入 cl-lab 的 SBCL REPL
#   scripts/run.sh swank [port]    起 swank 伺服器給 nvim/Conjure 連（預設 4005）
set -euo pipefail
QL="(load (merge-pathnames \"quicklisp/setup.lisp\" (user-homedir-pathname)))"
cmd="${1:-run}"; shift || true

case "$cmd" in
  run)
    # 把參數轉成 lisp 字串串列餵給 clingon
    args=""
    for a in "$@"; do args="$args \"$a\""; done
    sbcl --noinform --disable-debugger \
      --eval "$QL" \
      --eval '(ql:quickload :cl-lab :silent t)' \
      --eval "(clingon:run (cl-lab:cli-command) (list $args))" \
      --quit ;;
  test)
    sbcl --noinform --disable-debugger \
      --eval "$QL" \
      --eval '(ql:quickload :cl-lab/tests :silent t)' \
      --eval '(uiop:quit (if (fiveam:run! (quote cl-lab/tests:all-tests)) 0 1))' ;;
  build)
    sbcl --noinform --disable-debugger \
      --eval "$QL" \
      --eval '(ql:quickload :cl-lab :silent t)' \
      --eval '(asdf:make :cl-lab)' --quit
    echo "→ 產物：$(dirname "$0")/../build/cl-lab" ;;
  repl)
    sbcl --eval "$QL" --eval '(ql:quickload :cl-lab)' ;;
  swank)
    port="${1:-4005}"
    echo "swank 起在 127.0.0.1:$port —— nvim 開 .lisp 檔後按 ,cc 連（Ctrl-C 結束）"
    sbcl --eval "$QL" \
      --eval '(ql:quickload (list :swank :cl-lab) :silent t)' \
      --eval "(swank:create-server :port $port :dont-close t)" \
      --eval '(loop (sleep 1))' ;;
  *) echo "未知指令：$cmd（run|test|build|repl|swank）"; exit 1 ;;
esac
