--[[ argv.lua — 命令列掃描：把 argv 拆成旗標與位置參數（對齊 cli.cpp 的解析段）。

固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
特判；反射旗標（連線／取樣，見 flags.lua）吃下一個 argv；其餘當位置參數拼 prompt。「-」單獨＝
stdin 插入點，其餘 '-' 開頭＝未知旗標。掃描結果收成 ParsedArgs；遇 --help 或用法錯時回一個退出碼
交呼叫端直接返回（cli.main 只讀結果、不重演解析）。傳入 args＝Lua 的 arg 表（arg[1] 起為真參數）。]]
local flags = require("flags")
local internal = require("internal")

local M = {}

--- 空的 ParsedArgs（欄位對齊 core-py argv.py 的 ParsedArgs）。
local function new_parsed()
  return {
    raw_values = {}, prompt_parts = {}, media_specs = {}, tool_specs = {}, modality_specs = {},
    schema_text = nil, config_path = nil, media_out_dir = nil, system_text = nil,
    has_schema = false, has_config = false, has_system = false, stream = false,
  }
end

--- 掃描 argv，回 (ParsedArgs, nil)；遇 --help／用法錯回 (nil, 退出碼)。
function M.parse_argv(args)
  local p = new_parsed()
  local n = #args
  local i = 1

  local function need_value(flag)  -- 吃下一個 argv 當旗標的值；缺值回 nil（呼叫端據此回 EXIT_USAGE）
    if i + 1 > n then
      io.stderr:write(string.format("%s 缺少值（llm --help 看用法）\n", flag)); return nil
    end
    i = i + 1
    return args[i]
  end

  local no_more_flags = false
  while i <= n do
    local a = args[i]
    if no_more_flags then
      p.prompt_parts[#p.prompt_parts + 1] = a
    elseif a == "--" then
      no_more_flags = true
    elseif a == "--help" or a == "-h" then
      flags.print_usage(); return nil, internal.EXIT_OK
    elseif a == "--stream" then
      p.stream = true
    elseif a == "--image" or a == "--media" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.media_specs[#p.media_specs + 1] = v
    elseif a == "--schema" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.schema_text, p.has_schema = v, true
    elseif a == "--system" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.system_text, p.has_system = v, true
    elseif a == "--config" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.config_path, p.has_config = v, true
    elseif a == "--tool" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.tool_specs[#p.tool_specs + 1] = v
    elseif a == "--modality" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.modality_specs[#p.modality_specs + 1] = v
    elseif a == "--media-out" then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      p.media_out_dir = v
    elseif flags.FLAG_TO_FIELD[a] then
      local v = need_value(a); if v == nil then return nil, internal.EXIT_USAGE end
      local field, typ = flags.FLAG_TO_FIELD[a][1], flags.FLAG_TO_FIELD[a][2]
      p.raw_values[field] = { v, typ, a }
    elseif a:sub(1, 1) == "-" and a ~= "-" then  -- 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標
      io.stderr:write(string.format("未知旗標：%s（llm --help 看用法）\n", a)); return nil, internal.EXIT_USAGE
    else
      p.prompt_parts[#p.prompt_parts + 1] = a
    end
    i = i + 1
  end

  return p, nil
end

return M
