--[[ config.lua — 三層 config 來源解析（對齊 core/src/cli_config.cpp 的 load_into）。

來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本模組只處理「config 檔」這層；
命令列旗標覆寫在 cli.lua。config 檔路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
未列鍵靜默忽略（lenient；與 C++ glaze 的嚴格解析刻意分歧，見 core-py README 已知落差）。 ]]
local json = require("dkjson")
local internal = require("internal")
local flags = require("flags")

local M = {}

-- config 檔允許的鍵（對齊反射旗標欄位；未列鍵靜默忽略＝lenient）。
local CONFIG_KEYS = {}
for _, e in ipairs(flags.REFLECT_FLAGS) do CONFIG_KEYS[e[2]] = true end

--- ~/.config/llm/config.json（對齊 cli_config.cpp：靠 HOME）。無 HOME 回 nil。
function M.default_config_path()
  local home = os.getenv("HOME")
  if not home then return nil end
  return home .. "/.config/llm/config.json"
end

--- 三層 config 來源前二層（對齊 cli_config.cpp 的 load_into）。填 client、回退出碼。
function M.load_config(client, has_config, config_path)
  local named, cfg_path = false, nil
  if has_config then
    cfg_path, named = config_path, true
  elseif os.getenv(internal.CONFIG_ENV_VAR) then
    cfg_path, named = os.getenv(internal.CONFIG_ENV_VAR), true
  else
    cfg_path = M.default_config_path()
  end
  if not cfg_path then return internal.EXIT_OK end

  local body = internal.read_file_text(cfg_path)
  if not body then
    if named then  -- 明指卻讀不到＝用法錯（點名是誰指的路）
      local who = has_config and "--config" or internal.CONFIG_ENV_VAR
      io.stderr:write(string.format("讀不到檔案：%s（%s 指定的 config 檔）\n", cfg_path, who))
      return internal.EXIT_USAGE
    end
    return internal.EXIT_OK  -- 探測路徑讀不到＝沒設定檔，靜默用預設
  end

  local data = json.decode(body)
  if data == nil then
    io.stderr:write(string.format("config JSON 解析失敗（%s）\n", cfg_path))
    return internal.EXIT_USAGE
  end
  if type(data) == "table" then
    for k, v in pairs(data) do
      if CONFIG_KEYS[k] then client[k] = v end
    end
  end
  return internal.EXIT_OK
end

return M
