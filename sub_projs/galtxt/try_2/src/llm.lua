--- llm.lua — galtxt try_2：Lua 5.5 版統一 LLM 接口（schema 驅動，無 streaming）
---
--- 開發循環（playground 哲學）：改本檔 → 用 host.exe 跑腳本 → 直接呼叫。
---   local llm = dofile("llm.lua")
---   llm.base_url = "http://localhost:1234/v1"          -- 後端；預設讀 env AI_CORE_LLM_BASE_URL
---   llm.llm_entry{ prompt = "你是一隻貓娘，請回答我問題", temp = 0.1 }
---   llm.llm_entry{ infile = "q.txt", outfile = "a.txt", sys = "你是傲嬌貓娘",
---                  temp = 0.8, top_p = 0.9, max_tokens = 256 }
---
--- HTTP 走 io.popen("curl …") 捕捉輸出（跨後端 http/https 通吃）。請求用 Lua table 表達、
--- json.encode 序列化；回應 json.decode 成 table 導航——零手工 escape。
---
--- ★ 參數單一真相源＝下面的 M.schema：由它驅動 (1) 呼叫端參數驗證、(2) JSON 欄位對映、
---   (3) CLI 旗標名（cli.lua）、(4) CLI 值型別解析。新增一個參數＝schema 加一行，別處零改動。

-- JSON：vendored rxi/json.lua 0.1.2——json.encode / json.decode
-- 自解本檔目錄，載入同資料夾的 json.lua（location-independent，不綁 CWD）
local HERE = debug.getinfo(1, "S").source:sub(2):match("^(.*[/\\])") or ""
local json = dofile(HERE .. "json.lua")

local M = {}

-- ── 後端設定：Lua 有 os.getenv（不像 s7），預設讀 env，欄位可再 set 覆蓋
M.base_url = os.getenv("AI_CORE_LLM_BASE_URL") or "http://localhost:1234/v1"  -- 到 /v1 為止
M.model    = os.getenv("AI_CORE_LLM_MODEL")    or "local-model"               -- LM Studio 用當前載入模型
M.api_key  = os.getenv("AI_CORE_LLM_API_KEY")  or ""                          -- 有值才加 Authorization

-- 暫存請求 body（避開命令列引號地獄）：寫到系統 TEMP，絕對路徑 curl 吃得到、不髒專案目錄
local REQ_FILE = (os.getenv("TEMP") or os.getenv("TMP") or "."):gsub("[/\\]+$", "") .. "/galtxt_llm_req.json"
M.req_file = REQ_FILE                     -- 公開給 test 回讀驗證用

-- ── ★ 參數 schema：唯一真相源。每列＝{名稱, JSON鍵或"ctrl", 值型別}
--    JSON鍵或"ctrl"："ctrl"＝控制參數（本體流程手動處理）；其它字串＝直接塞進 OpenAI 請求的 JSON 欄位鍵
--    值型別："str"/"num"/"int"，給 cli.lua 解析命令列字串用（Lua 端呼叫直接傳型別正確的值）
--    要加參數（例如 stop、logit_bias）：這裡加一行即可，驗證／塞入／CLI 旗標自動跟上。
M.schema = {
  {"prompt",           "ctrl",              "str"},
  {"infile",           "ctrl",              "str"},   -- 讀檔內容，接在 prompt 後面（CLI: --infile）
  {"outfile",          "ctrl",              "str"},   -- completion 寫檔（CLI: --outfile）
  {"sys",              "ctrl",              "str"},   -- system role
  {"model",            "ctrl",              "str"},
  {"base_url",         "ctrl",              "str"},
  {"api_key",          "ctrl",              "str"},
  {"lua",              "ctrl",              "bool"},  -- ★ 把回應內容當 Lua table 字面量 sandbox load 成原生 table（CLI: --lua，無值旗標）
  {"streaming",        "ctrl",              "bool"},  -- ★ 串流模式：即時回 {ok=…}，內容經 callback 持續餵（CLI: --streaming）
  {"callback",         "ctrl",              "func"},  -- 串流回呼 function(chunk)；只能 Lua 端傳（CLI 無此旗標）
  {"chunk_chars",      "ctrl",              "int"},   -- 累積幾個 UTF-8 字才呼叫一次 callback（預設：每片就緒即吐）
  {"temp",             "temperature",       "num"},
  {"top_p",            "top_p",             "num"},
  {"top_k",            "top_k",             "int"},   -- ⚠ 非 OpenAI 標準，本地多支援、雲端未必
  {"max_tokens",       "max_tokens",        "int"},
  {"n",                "n",                 "int"},
  {"seed",             "seed",              "int"},
  {"presence_penalty", "presence_penalty",  "num"},
  {"frequency_penalty","frequency_penalty", "num"},
}

-- 由 schema 建「合法參數名」集合，供呼叫端驗證
local valid = {}
for _, e in ipairs(M.schema) do valid[e[1]] = true end

-- ── 小工具
local function slurp(path)                     -- 讀整個檔成字串（binary 模式，UTF-8 原樣）
  local f = assert(io.open(path, "rb"))
  local s = f:read("a"); f:close(); return s
end
local function spit(path, str)                 -- 字串寫檔
  local f = assert(io.open(path, "wb")); f:write(str); f:close()
end
local function capture(cmd)                    -- 跑指令、捕捉 stdout（含 2>&1）
  local p = assert(io.popen(cmd, "r"))
  local out = p:read("a"); p:close(); return out
end

-- ── UTF-8 安全切分：把字串分成「完整字前綴 / 殘餘不完整位元組 / 完整字數」，絕不切半個字。
local function utf8_split_complete(s)
  local n, bad = utf8.len(s)                   -- 全完整時回字數；遇不完整回 nil + 第一個壞位元組位置
  if n then return s, "", n end
  local head = s:sub(1, bad - 1)
  return head, s:sub(bad), (utf8.len(head) or 0)
end

-- ── 串流傳輸：curl -N 逐行讀 SSE，delta.content 累積到門檻→呼叫 callback（UTF-8 對齊）。
--    chunk_chars 有值＝每滿這麼多字吐一次；沒值＝每片就緒的完整字即吐。收尾把殘留全吐。
--    回傳 {ok=bool, err=?}——內容不放回傳值，全走 callback（依設計）。
local function run_stream(cmd, callback, chunk_chars)
  local p, oerr = io.popen(cmd, "r")
  if not p then return { ok = false, err = "popen 失敗：" .. tostring(oerr) } end
  local buf = ""
  local function emit_ready(force)
    while true do
      local head, tail, count = utf8_split_complete(buf)
      if count == 0 then break end
      local take
      if chunk_chars and chunk_chars > 0 and not force then
        if count < chunk_chars then break end
        take = chunk_chars
      else
        take = count                           -- 無門檻 或 收尾：把完整字全吐
      end
      local cut = utf8.offset(head, take + 1) or (#head + 1)
      callback(head:sub(1, cut - 1))
      buf = head:sub(cut) .. tail
    end
  end
  for line in p:lines() do
    local data = line:match("^data:%s?(.*)")   -- SSE：data: {…} / data: [DONE]
    if data then
      if data == "[DONE]" then break end
      local dok, chunk = pcall(json.decode, data)
      if dok and chunk.choices and chunk.choices[1] and chunk.choices[1].delta then
        local piece = chunk.choices[1].delta.content
        if type(piece) == "string" and #piece > 0 then
          buf = buf .. piece
          emit_ready(false)
        end
      end
    end
  end
  emit_ready(true)                             -- 收尾：剩下的完整字全吐
  if #buf > 0 then callback(buf) end           -- 連殘餘不完整位元組也吐（罕見；串流正常收在字邊界）
  p:close()
  return { ok = true }
end

-- ── ★ Lua 格式輸出：把模型吐的「Lua table 字面量」sandbox load 成原生 table（零 JSON parse）。
--    比讓模型吐 JSON 更順：Lua 語法對 LLM 更寬容（尾逗號、註解），多行台詞用 [[…]] 免跳脫；
--    而且回來就是 runtime 原生 table，直接導航。★ 安全：load 模式 "t"（只吃文字、不吃 bytecode）
--    ＋空環境 env（沒有 os/io/load，只能建純資料 table），杜絕 LLM 輸出變成任意程式碼執行。
function M.parse_lua(src)
  -- 去掉 markdown code fence（模型常包 ```lua … ```）
  src = src:gsub("^%s*```%w*%s*\n?", ""):gsub("\n?```%s*$", "")
  -- 若沒 return 開頭但整體是一個 {…}，補上 return
  if not src:match("^%s*return%s") and src:match("^%s*%b{}%s*$") then
    src = "return " .. src
  end
  local chunk, err = load(src, "=llm-lua", "t", {})   -- "t"＝純文字；{}＝空沙箱環境
  if not chunk then return nil, "load 失敗：" .. tostring(err) end
  local ok, val = pcall(chunk)
  if not ok then return nil, "執行失敗：" .. tostring(val) end
  return val
end

-- ── 把 Lua 值序列化回可讀的 Lua 字面量（給 --lua 的 CLI 回顯／存檔用；同像性 round-trip）。
function M.dump(v, indent)
  indent = indent or ""
  local t = type(v)
  if t == "string" then return string.format("%q", v)          -- %q 產生 Lua 安全字串（中文原樣、控制字元跳脫）
  elseif t == "number" or t == "boolean" or t == "nil" then return tostring(v)
  elseif t == "table" then
    local ni, parts, done = indent .. "  ", {}, {}
    for idx = 1, #v do parts[#parts + 1] = ni .. M.dump(v[idx], ni); done[idx] = true end
    for k, val in pairs(v) do
      if not (type(k) == "number" and done[k]) then
        local key = (type(k) == "string" and k:match("^[%a_][%w_]*$"))
            and k or ("[" .. M.dump(k, ni) .. "]")
        parts[#parts + 1] = ni .. key .. " = " .. M.dump(val, ni)
      end
    end
    if #parts == 0 then return "{}" end
    return "{\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "}"
  end
  return string.format("%q", tostring(v))
end

-- ── 主入口：吃一個 opts table（鍵＝schema 名稱）。等價於 s7 版的 llm-entry。
function M.llm_entry(opts)
  opts = opts or {}
  -- 驗證未知鍵（對齊 s7 版「未知 keyword 被擋」）
  for k in pairs(opts) do
    if not valid[k] then error("llm-entry: 未知參數 '"..tostring(k).."'", 2) end
  end
  -- 1. 組 prompt：prompt 本體 ＋（infile 檔接在後面，中間補換行）
  local prompt, infile = opts.prompt, opts.infile
  local body
  if prompt and infile then body = prompt .. "\n" .. slurp(infile)
  elseif prompt then body = prompt
  elseif infile then body = slurp(infile)
  else error("llm-entry: 需要 prompt 或 infile", 2) end
  local model = opts.model    or M.model
  local base  = opts.base_url or M.base_url
  local key   = opts.api_key  or M.api_key
  -- 2. messages（有 sys 就前置 system role）；純序列 table，json.encode 自動編成陣列
  local user = { role = "user", content = body }
  local msgs = opts.sys
      and { { role = "system", content = opts.sys }, user }
      or  { user }
  -- 3. 請求 table：固定欄位 ＋ schema 驅動的取樣參數（非 ctrl、有給才塞）
  --    stream 欄位跟著 streaming 開關（OpenAI 相容端點靠它回 SSE）
  local req = { model = model, messages = msgs, stream = opts.streaming and true or false }
  for _, e in ipairs(M.schema) do
    local name, kind = e[1], e[2]
    if kind ~= "ctrl" and opts[name] ~= nil then req[kind] = opts[name] end
  end
  -- 4. 寫請求檔 → curl
  spit(REQ_FILE, json.encode(req))
  local auth = (#key > 0) and (' -H "Authorization: Bearer ' .. key .. '"') or ""
  local base_cmd = 'curl -sS "' .. base .. '/chat/completions"'
      .. ' -H "Content-Type: application/json"' .. auth
      .. ' --data-binary @' .. REQ_FILE
  -- ── ★ 串流模式：需 callback；即時回 {ok=…}，內容全走 callback（不回內容本體）
  if opts.streaming then
    if type(opts.callback) ~= "function" then
      error("llm-entry: streaming=true 需要 callback 函式", 2)
    end
    return run_stream(base_cmd .. " -N", opts.callback, opts.chunk_chars)
  end
  local resp = capture(base_cmd .. " 2>&1")
  -- 5. 解析 choices[1].message.content
  local ok, j = pcall(json.decode, resp)
  local content
  if ok and j and j.choices and j.choices[1] and j.choices[1].message then
    content = j.choices[1].message.content
  end
  if type(content) ~= "string" then
    io.stderr:write("llm-entry: 回應無 choices[1].message.content。原始回應：\n" .. resp .. "\n")
    return nil
  end
  -- token 計量（有才印）
  if ok and j.usage and j.usage.total_tokens then
    io.stderr:write("[meter] total_tokens=" .. tostring(j.usage.total_tokens) .. "\n")
  end
  -- ★ Lua 格式輸出：把 content 當 Lua table 字面量 sandbox load 成原生 table
  if opts.lua then
    local tbl, perr = M.parse_lua(content)
    if not tbl then
      io.stderr:write("llm-entry: 內容非合法 Lua table（" .. tostring(perr) .. "）。原始內容：\n" .. content .. "\n")
      return nil
    end
    if opts.outfile then spit(opts.outfile, content) end   -- outfile 寫原始 Lua 源碼
    return tbl
  end
  if opts.outfile then spit(opts.outfile, content); return opts.outfile end
  return content
end

-- ── ★ (A) 縫：Lua table 進、Lua table 出。JSON/HTTP 是這條縫「背後」的實作細節——
--    現由 Lua（json.lua）做，將來可換成 C++ 原生函式，而呼叫端 `llm.ask{…}` 一行都不用改。
--    非串流：回模型答案的原生 table（自動開 lua 解析）；缺內容/解析失敗回 nil。
--    串流（streaming=true+callback）：回 {ok=bool, err=?}，內容全走 callback。
function M.ask(opts)
  local o = {}
  for k, v in pairs(opts or {}) do o[k] = v end
  if o.lua == nil then o.lua = true end        -- (A) 預設吐 table
  return M.llm_entry(o)
end

return M
