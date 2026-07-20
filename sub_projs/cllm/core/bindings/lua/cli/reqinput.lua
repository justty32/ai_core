--[[ reqinput.lua — 把 CLI 的請求類旗標驗證／組成 llm.ask 的請求輸入（cli.cpp 組請求段）。

⚠ 與 C++ llm 刻意分歧（同 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON 文字」
（同 --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
--image/--media 的三分流取值在 media.lua。本檔把這些旗標收成 llm.ask 要的 schema／tools／
modalities／media 四份輸入，並驗 --media-out 目錄。

注意 tool 的 parameters：llm.ask 綁定要 JSON 字串，故把解出的物件重新 encode 成字串再交。 ]]
local json = require("dkjson")
local internal = require("internal")
local media = require("media")

local M = {}

--- 從 ParsedArgs 組請求輸入，回 (RequestInputs, EXIT_OK)；驗證失敗回 (nil, 退出碼)。
function M.build_request_inputs(p)
  local r = { schema_body = nil, tool_defs = {}, modality_items = {}, media_items = {} }

  if p.has_schema then
    if json.decode(p.schema_text) == nil then  -- 只驗合法；字面原樣交內核嵌入
      io.stderr:write("--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）\n")
      return nil, internal.EXIT_USAGE
    end
    r.schema_body = p.schema_text
  end

  for _, spec in ipairs(p.tool_specs) do  -- 字面 tool def JSON；需 name 與非空 parameters
    local td = json.decode(spec)
    if td == nil then
      io.stderr:write("--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）\n")
      return nil, internal.EXIT_USAGE
    end
    if type(td) ~= "table" or not td.name or td.parameters == nil then
      io.stderr:write("--tool 缺 name 或 parameters\n"); return nil, internal.EXIT_USAGE
    end
    -- parameters 綁定要字串：本就是字串則直用，否則把物件 encode 回 JSON 字串。
    local params = type(td.parameters) == "string" and td.parameters or json.encode(td.parameters)
    r.tool_defs[#r.tool_defs + 1] = { name = td.name, description = td.description or "", parameters = params }
  end

  for _, spec in ipairs(p.modality_specs) do  -- 「名」或「名=<字面JSON>」
    local name, cfg = spec:match("^([^=]*)=(.*)$")
    if name == nil then name = spec end  -- 無 '='：純模態名、無 config
    if name == "" then
      io.stderr:write("--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）\n")
      return nil, internal.EXIT_USAGE
    end
    if cfg ~= nil and json.decode(cfg) == nil then  -- 有 '='：cfg 收字面 JSON，驗合法
      io.stderr:write("--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）\n")
      return nil, internal.EXIT_USAGE
    end
    r.modality_items[#r.modality_items + 1] = { name = name, config = cfg }
  end

  for _, spec in ipairs(p.media_specs) do  -- 三分流：URL 直通 / .json 描述子直通 / 二進位圖檔讀檔編碼
    local item = media.build_media_item(spec)
    if item == nil then return nil, internal.EXIT_USAGE end
    r.media_items[#r.media_items + 1] = item
  end

  if p.media_out_dir and not internal.is_dir(p.media_out_dir) then
    io.stderr:write("--media-out 不是可用目錄：" .. p.media_out_dir .. "\n")
    return nil, internal.EXIT_USAGE
  end

  return r, internal.EXIT_OK
end

return M
