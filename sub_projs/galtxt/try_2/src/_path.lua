-- _path.lua — 路徑工具：定位本專案根（try_2）的絕對路徑、組 curl 吃的 file:// URL。
-- 給 test/ 與 examples/ 的離線 fixture 用；跨 CWD、跨平台（Windows / Linux）。
-- 靠 debug.getinfo 自解本檔位址，相對時用 cwd 補齊，再正規化掉 . 與 ..。
local M = {}

-- 平台偵測：package.config 首字＝目錄分隔符（Windows "\"、Unix "/"）
local IS_WIN = package.config:sub(1, 1) == "\\"

local function getcwd()
  local p = io.popen(IS_WIN and "cd" or "pwd")   -- Windows cmd 用 cd；Unix sh 用 pwd
  local d = p:read("l"); p:close()
  return (d or "."):gsub("[/\\]+$", "")
end

-- 正規化：統一正斜線、解析掉 . 與 ..，保留 Unix 前導 "/"（絕對路徑）
local function normalize(p)
  p = p:gsub("\\", "/")
  local lead = p:match("^/") and "/" or ""       -- Unix 絕對路徑的前導斜線
  local out = {}
  for seg in p:gmatch("[^/]+") do
    if seg == ".." then out[#out] = nil
    elseif seg ~= "." then out[#out + 1] = seg end
  end
  return lead .. table.concat(out, "/")
end
M.normalize = normalize

-- 本檔（src/_path.lua）所在目錄，絕對化（Windows 磁碟機字母 或 Unix 前導斜線＝已絕對）
local self_dir = debug.getinfo(1, "S").source:sub(2):match("^(.*[/\\])") or ""
if not (self_dir:match("^%a:[/\\]") or self_dir:match("^[/\\]")) then
  self_dir = getcwd() .. "/" .. self_dir
end

M.root = normalize(self_dir .. "/..")            -- src/ 的上一層 = try_2 專案根（絕對、正規化）

-- 絕對路徑 → file:// URL：Unix（/home/…）用 file://+path；Windows（C:/…）用 file:///+path
function M.fileurl(abspath)
  abspath = normalize(abspath)
  return (abspath:sub(1, 1) == "/" and "file://" or "file:///") .. abspath
end
function M.fixture(name) return M.fileurl(M.root .. "/test/fixtures/" .. name) end

return M
