#!/usr/bin/env bash
# run.sh —— lisp-try-1 開發用小工具（照 cl-lab 慣例）。用法：
#   scripts/run.sh run [args...]   直接跑 CLI（開發主循環）
#   scripts/run.sh test            跑離線測（fiveam）
#   scripts/run.sh build           編出獨立執行檔 build/lisp-try-1
#   scripts/run.sh repl            開一個已載入 lisp-try-1 的 SBCL REPL
#
# 註：會自動 source ~/dev/cllm/env.sh（若 CLLM_LISP 尚未設），
#     好讓執行期 (load CLLM_LISP) 找得到 cllm binding、libcllm.so。
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# 掛上 cllm 開發環境（CLLM_LISP / LIBCLLM / PATH…），除非呼叫端已設好
if [ -z "${CLLM_LISP:-}" ] && [ -f "$HOME/dev/cllm/env.sh" ]; then
  # shellcheck disable=SC1091
  . "$HOME/dev/cllm/env.sh"
fi

QL="(load (merge-pathnames \"quicklisp/setup.lisp\" (user-homedir-pathname)))"
REG="(push #p\"$HERE/\" asdf:*central-registry*)"   # 讓 asdf/ql 找得到本專案的 .asd
cmd="${1:-run}"; shift || true

case "$cmd" in
  run)
    args=""
    for a in "$@"; do args="$args \"$a\""; done
    sbcl --noinform --disable-debugger \
      --eval "$QL" \
      --eval "$REG" \
      --eval '(ql:quickload :lisp-try-1 :silent t)' \
      --eval "(clingon:run (lisp-try-1:cli-command) (list $args))" \
      --quit ;;
  test)
    sbcl --noinform --disable-debugger \
      --eval "$QL" \
      --eval "$REG" \
      --eval '(ql:quickload :lisp-try-1/tests :silent t)' \
      --eval '(uiop:quit (if (fiveam:run! (quote lisp-try-1/tests:all-tests)) 0 1))' ;;
  build)
    sbcl --noinform --disable-debugger \
      --eval "$QL" \
      --eval "$REG" \
      --eval '(ql:quickload :lisp-try-1 :silent t)' \
      --eval '(asdf:make :lisp-try-1)' --quit
    echo "→ 產物：$HERE/build/lisp-try-1" ;;
  repl)
    sbcl --eval "$QL" --eval "$REG" --eval '(ql:quickload :lisp-try-1)' ;;
  *) echo "未知指令：$cmd（run|test|build|repl）"; exit 1 ;;
esac
