#!/usr/bin/env lua
-- main.lua —— CLI 進入點：讀 endpoint/api-key/model → 呼 llm.ask → 印結果 / 分流錯誤。
--
-- 跑法（通常經 scripts/up.sh，它會先備好 proxy＋憑證再帶環境變數進來）：
--   lua main.lua 用一句話介紹你自己
--   lua main.lua --stream 寫一首短詩
--   lua main.lua --check                       # 離線自檢：binding 就緒？環境變數齊？
--   APP_ENDPOINT=... APP_API_KEY=... lua main.lua 你好
--
-- 環境變數（scripts/up.sh 會設；也可自己給）：
--   APP_ENDPOINT  cllm endpoint（預設本機 proxy）
--   APP_API_KEY   Bearer token
--   APP_MODEL     模型 id（預設 claude-opus-4-8）

-- 讓 require("src.app") 找得到本專案模組（不論從哪個工作目錄呼叫）。
local here = (arg[0] and arg[0]:match("^(.*)/[^/]*$")) or "."
package.path = here .. "/?.lua;" .. here .. "/?/init.lua;" .. package.path
local app = require("src.app")

local default_endpoint = "http://127.0.0.1:8787/v1/chat/completions"
local default_model    = "claude-opus-4-8"

local function env(k, d)
  local v = os.getenv(k)
  if v and v ~= "" then return v end
  return d
end

local usage = [[
lua-try-1 —— 用 Lua 打 Anthropic API（經 cllm binding → anthropic-proxy）

用法：
  lua main.lua [選項] <prompt 各詞...>

選項：
  -s, --stream        串流輸出（逐段印）。
      --check         離線自檢（不觸網），看 binding／環境是否備妥。
      --endpoint URL  覆寫 endpoint（預設 $APP_ENDPOINT 或本機 proxy）。
  -m, --model  ID     覆寫模型 id（預設 $APP_MODEL 或 claude-opus-4-8）。
  -h, --help          顯示本說明。
]]

-- ── 簡單參數解析 ──────────────────────────────────────────────
local function parse(argv)
  local o = { words = {}, stream = false, check = false, help = false }
  local i = 1
  while i <= #argv do
    local a = argv[i]
    if a == "--stream" or a == "-s" then o.stream = true
    elseif a == "--check" then o.check = true
    elseif a == "-h" or a == "--help" then o.help = true
    elseif a == "--endpoint" then i = i + 1; o.endpoint = argv[i]
    elseif a == "--model" or a == "-m" then i = i + 1; o.model = argv[i]
    else o.words[#o.words + 1] = a end
    i = i + 1
  end
  return o
end

-- ── 離線自檢：不觸網，回報 binding 與環境狀態 ──────────────────
local function do_check()
  print("lua-try-1 自檢")
  print(("  cllm Lua binding：%s"):format(
    app.binding_ready() and "就緒（require(\"llm\") OK）"
      or "尚未就緒 —— source ~/dev/cllm/env.sh 掛上 LUA_CPATH 後再試"))
  print(("  APP_ENDPOINT：%s"):format(env("APP_ENDPOINT", default_endpoint .. "（預設）")))
  print(("  APP_MODEL：   %s"):format(env("APP_MODEL", default_model .. "（預設）")))
  print(("  APP_API_KEY： %s"):format(
    os.getenv("APP_API_KEY") and "已設（不外洩內容）" or "未設 → 會撞 401"))
  print("  失敗分流自測：")
  local cases = {
    { "後端錯誤 (HTTP 401): Unauthorized", "auth" },
    { "curl: (7) Failed to connect: Connection refused", "sidecar" },
    { "something else broke", "other" },
  }
  for _, c in ipairs(cases) do
    local kind = app.classify_error(c[1])
    print(("    %-45s → %s %s"):format(c[1], kind, kind == c[2] and "OK" or "MISMATCH"))
  end
  os.exit(0)
end

-- ── 主流程 ────────────────────────────────────────────────────
local opt = parse(arg)
if opt.help then io.write(usage); os.exit(0) end
if opt.check then do_check() end

local prompt = table.concat(opt.words, " ")
if prompt == "" then
  io.stderr:write("沒給 prompt。範例：lua main.lua 你好   （或 --check 自檢）\n")
  os.exit(2)
end

local endpoint = opt.endpoint or env("APP_ENDPOINT", default_endpoint)
local model    = opt.model    or env("APP_MODEL", default_model)
local api_key  = os.getenv("APP_API_KEY")
local stream   = opt.stream

local r = app.ask(prompt, {
  endpoint = endpoint,
  api_key  = api_key,
  model    = model,
  stream   = stream,
  -- 只在串流時逐段印；非串流靠結尾一次 print(r.text)，避免 on_delta＋print 重覆輸出。
  on_delta = stream and function(piece) io.write(piece); io.flush(); return false end or nil,
})

if r.ok then
  if stream then print() else print(r.text) end
  os.exit(0)
else
  -- 失敗：印分流訊息到 stderr，退出碼帶語意（1=一般/憑證、3=sidecar、4=no-binding）
  io.stderr:write(("✗ 失敗（%s）：\n    %s\n"):format(r.kind, r.error))
  local code = ({ sidecar = 3, ["no-binding"] = 4 })[r.kind] or 1
  os.exit(code)
end
