-- _path.lua — 路徑工具：定位本專案根（try_2）的絕對路徑、組 curl 吃的 file:// URL。
-- 給 test/ 與 examples/ 的離線 fixture 用；跨 CWD——靠 debug.getinfo 自解本檔位址，
-- 相對時用 cwd 補齊（Windows：io.popen("cd") 印當前絕對目錄），再正規化掉 . 與 ..。
local M = {}

local function getcwd()
  local p = io.popen("cd")                 -- Windows cmd 內建：印當前絕對路徑
  local d = p:read("l"); p:close()
  return (d or "."):gsub("[/\\]+$", "")
end

-- 正規化：統一正斜線、解析掉 . 與 ..（curl file:// 不保證會解 ..）
local function normalize(p)
  local out = {}
  for seg in p:gsub("\\", "/"):gmatch("[^/]+") do
    if seg == ".." then out[#out] = nil
    elseif seg ~= "." then out[#out + 1] = seg end
  end
  return table.concat(out, "/")
end
M.normalize = normalize

-- 本檔（src/_path.lua）所在目錄，絕對化
local self_dir = debug.getinfo(1, "S").source:sub(2):match("^(.*[/\\])") or ""
if not (self_dir:match("^%a:[/\\]") or self_dir:match("^[/\\]")) then
  self_dir = getcwd() .. "/" .. self_dir
end

M.root = normalize(self_dir .. "/..")      -- src/ 的上一層 = try_2 專案根（絕對、正規化）

function M.fileurl(abspath) return "file:///" .. normalize(abspath) end
function M.fixture(name) return M.fileurl(M.root .. "/test/fixtures/" .. name) end

return M
