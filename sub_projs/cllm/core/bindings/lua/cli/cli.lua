--[[ cli.lua — 薄 CLI 外殼：把命令列組成一次 llm.ask 的發問（鏡像 C++ 的 `llm` CLI）。

只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給 Lua binding 的 llm.ask。
周邊拆到姊妹模組（皆對齊 C++ 的分檔）：internal（cli_internal.hpp）＝退出碼／env 鍵／檔案讀取；
flags（cli_flags.cpp）＝反射旗標表＋print_usage；argv（cli.cpp 解析段）；config（cli_config.cpp）＝
三層 config 解析；media＝--media 三分流取值；reqinput（cli.cpp 組請求段）；output（cli_output.cpp）＝
出口 handlers（Sink）。本檔（對齊 cli.cpp）只留 main 接線。

流程：(1) 掃 argv → (2) 定 prompt（位置參數×stdin，「-」＝插入點）→ (3) 組 client 設定
（預設→config→旗標覆寫）→ (4) 組請求輸入＋呼叫 llm.ask，output 走 Sink。

退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗（傳輸／後端／媒體落檔失敗）；130 取消。]]
local llm = require("llm")
local internal = require("internal")
local argv = require("argv")
local config = require("config")
local reqinput = require("reqinput")
local output = require("output")

local M = {}

-- 反射旗標的字串值 → 目標型別；型別錯回 nil, 錯誤訊息。
local function coerce(val, typ)
  if typ == "str" then return val end
  local num = tonumber(val)
  if num == nil then return nil, (typ == "int") and "整數" or "小數" end
  if typ == "int" then
    local iv = math.tointeger(num)
    if iv == nil then return nil, "整數" end
    return iv
  end
  return num  -- float
end

--- CLI 進入點；args＝Lua 的 arg 表（arg[1] 起為真參數）。回退出碼。
function M.main(args)
  -- ── (1) 掃描 argv ──
  local p, ec = argv.parse_argv(args)
  if p == nil then return ec end

  -- ── (2) prompt：位置參數與導管 stdin 可合體 ──
  local has_dash = false
  for _, part in ipairs(p.prompt_parts) do if part == "-" then has_dash = true end end
  local stdin_text = ""
  if not internal.stdin_is_tty() then  -- 只在被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）
    stdin_text = (io.read("a") or ""):gsub("[\r\n]+$", "")
  elseif has_dash then
    io.stderr:write("「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（llm --help 看用法）\n")
    return internal.EXIT_USAGE
  end

  local pieces = {}
  for _, part in ipairs(p.prompt_parts) do pieces[#pieces + 1] = (part == "-") and stdin_text or part end
  local prompt = table.concat(pieces, " ")
  if not has_dash and stdin_text ~= "" then  -- 沒寫「-」而兩者都有＝prompt＋空行＋stdin
    prompt = (prompt == "") and stdin_text or (prompt .. "\n\n" .. stdin_text)
  end
  if prompt == "" then
    io.stderr:write("缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）\n")
    return internal.EXIT_USAGE
  end

  -- ── (3) 組 client 設定：內建預設 → config → 反射旗標覆寫 ──
  local client = {}
  ec = config.load_config(client, p.has_config, p.config_path)
  if ec ~= internal.EXIT_OK then return ec end
  for field, spec in pairs(p.raw_values) do
    local val, typ, flag = spec[1], spec[2], spec[3]
    local coerced, kind = coerce(val, typ)
    if coerced == nil and kind then
      io.stderr:write(string.format("%s 需要%s（給了「%s」）\n", flag, kind, val))
      return internal.EXIT_USAGE
    end
    client[field] = coerced
  end

  -- ── (4) 組請求輸入 ＋ 呼叫綁定 ──
  local r
  r, ec = reqinput.build_request_inputs(p)
  if r == nil then return ec end

  local sink = output.new(p.media_out_dir)
  local opts = {
    prompt = prompt, endpoint = client.endpoint,
    system = p.has_system and p.system_text or nil,
    api_key = client.api_key, model = client.model, timeout_ms = client.timeout_ms,
    temperature = client.temperature, top_p = client.top_p,
    presence_penalty = client.presence_penalty, frequency_penalty = client.frequency_penalty,
    max_tokens = client.max_tokens, seed = client.seed,
    stream = p.stream, schema = r.schema_body,
    tools = (#r.tool_defs > 0) and r.tool_defs or nil,
    media = (#r.media_items > 0) and r.media_items or nil,
    modalities = (#r.modality_items > 0) and r.modality_items or nil,
    on_delta = sink.on_delta, on_tool = sink.on_tool,
    on_media = sink.on_media, on_error = sink.on_error,
  }
  local text, err = llm.ask(opts)
  if text == nil then  -- 綁定失敗回 nil, errmsg
    if err == "cancelled" then return internal.EXIT_CANCEL end
    sink.had_error = true  -- 保險：即使 on_error 沒觸發也算請求失敗
  end

  local ok = not sink.had_error
  if sink.wrote_text and ok then io.stdout:write("\n"); io.stdout:flush() end  -- 補尾端換行
  if not ok then return internal.EXIT_REQUEST end
  return sink.media_err and internal.EXIT_REQUEST or internal.EXIT_OK  -- 媒體落檔失敗＝結果真掉了
end

return M
