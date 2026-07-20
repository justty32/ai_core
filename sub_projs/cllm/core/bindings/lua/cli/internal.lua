--[[ internal.lua — CLI 共用的退出碼、環境變數鍵、檔案讀取＋小工具（對齊 core/src/cli_internal.hpp）。

葉模組：只依標準庫、不 require 套件內其他 cli 模組，作為其餘 cli 模組（flags/config/media/
output/argv/reqinput/cli）的共用底，把依賴收成一張無環 DAG。額外收了兩件 Lua 缺的底層小工具：
b64decode（media 描述子解 base64）與 stdin_is_tty（判互動終端，靠 `test -t 0`）。]]
local M = {}

-- 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
M.EXIT_OK = 0
M.EXIT_USAGE = 1
M.EXIT_REQUEST = 2
M.EXIT_CANCEL = 130

M.CONFIG_ENV_VAR = "LLM_CLI_CONFIG"  -- 對齊 cli_internal.hpp 的 kConfigEnvVar

--- 整檔讀成 raw bytes（媒體圖檔等）。讀不到回 nil，由呼叫端轉退出碼。
function M.read_file_bytes(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local data = f:read("a")
  f:close()
  return data or ""
end

--- 整檔讀成文字（config／media 描述子等）。Lua string 二進位安全，與 bytes 同路。讀不到回 nil。
function M.read_file_text(path)
  return M.read_file_bytes(path)
end

--- 目錄判定（stdlib 無 stat）：能開但讀首位元組回 EISDIR(21)＝目錄；開不到／可讀＝非目錄。
function M.is_dir(path)
  local f = io.open(path, "rb")
  if not f then return false end
  local _, _, code = f:read(1)
  f:close()
  return code == 21
end

--- stdin 是否互動終端（stdlib 無 isatty）：借 POSIX `test -t 0`，反映本行程繼承的 fd0。
function M.stdin_is_tty()
  return os.execute("test -t 0") == true
end

-- 純 Lua base64 解碼（media 描述子 {mime,data} 的 data 欄；stdlib 無 base64）。回 raw 字串。
local B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
function M.b64decode(data)
  data = tostring(data):gsub("[^" .. B64 .. "=]", "")
  return (data:gsub("=", ""):gsub("[" .. B64 .. "]", function(x)
    local bits, f = "", B64:find(x, 1, true) - 1
    for i = 6, 1, -1 do bits = bits .. (f % 2 ^ i - f % 2 ^ (i - 1) > 0 and "1" or "0") end
    return bits
  end):gsub("%d%d%d?%d?%d?%d?%d?%d?", function(x)
    if #x ~= 8 then return "" end
    local c = 0
    for i = 1, 8 do c = c + (x:sub(i, i) == "1" and 2 ^ (8 - i) or 0) end
    return string.char(c)
  end))
end

return M
