#!/usr/bin/env sh
# llme.sh — 轉發入口（folder-as-callable 的外殼）：把呼叫原樣轉給 llme/_exec。
# 用法同 _exec：llme.sh <endpoint> [llm 參數...]，例：llme.sh opus --stream 你好
# 放在 PATH 上（或 symlink 成 `llme`）即可當一支命令用。
# readlink -f 解開 symlink：放到 PATH 上（~/.local/bin/llme → 本檔）時，$0 是
# symlink 路徑、非真身，需解回真身才找得到同層的 llme/_exec。
self=$(readlink -f -- "$0")
here=$(CDPATH= cd -- "$(dirname -- "$self")" && pwd)
exec "$here/llme/_exec" "$@"
