#!/usr/bin/env python3
# _echo.py — 跨平台 echo 蟬（給 Windows 的離線 dry-run 用）。
#
# 原 handy 用 `LLME_LLM=echo`／`WF_CLAUDE=echo` 把外部命令換成 echo，看轉出的參數。
# Windows 無 echo.exe（cmd 內建），list-form subprocess 找不到——_handy._resolve_external
# 把 `echo` 轉接到本蟬，輸出對齊 POSIX `echo <args>`：各參數以單一空白相接、結尾換行。
import sys

print(" ".join(sys.argv[1:]))
