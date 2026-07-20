#!/usr/bin/env lua
--[[ main.lua — Lua 薄 CLI 進入點（對齊 cli.cpp 的 _entry／main）。

跑：source ~/dev/cllm/env.sh 後  lua cli/main.lua [旗標...] [prompt...]
把本檔所在目錄加進 package.path 好 require 姊妹模組（llm/dkjson 仍由 env 的 LUA_PATH/CPATH 定位），
載入 cli 跑 main、以其退出碼結束。SIGINT 由綁定內核處理（回 "cancelled" → 退 130，見 cli.lua）。]]
local src = debug.getinfo(1, "S").source:match("^@(.*)$") or ""
local here = src:match("^(.*)[/\\]") or "."
package.path = here .. "/?.lua;" .. package.path

local cli = require("cli")
os.exit(cli.main(arg))
