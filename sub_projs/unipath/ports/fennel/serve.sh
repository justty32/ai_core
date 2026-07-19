#!/usr/bin/env bash
# serve.sh —— 把 .fnl 編成 .lua，再用 luajit 跑起 9P2000 server。
# 用法： ./serve.sh <port>
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
for f in codec env envio ninep dispatch fsock server main; do
  fennel --compile "$f.fnl" > "$f.lua"
done
exec luajit main.lua serve "${1:-5641}"
