--- cli.lua — galtxt try_2：由 llm.lua 的 M.schema「生成」--flag CLI 薄殼
---
--- 「函式參數 ⇄ CLI 旗標由 schema 生成」洞見的落地——旗標名、值型別解析全從同一張 schema 推出，
--- 這裡零手寫參數列。host.exe 已把命令列綁進全域 arg 表，本檔解析它 → 呼叫 llm.llm_entry。
---
--- 用（cwd 需在 try_2）：
---   host.exe cli.lua --prompt "你是一隻貓娘" --temp 0.7
---   host.exe cli.lua --infile q.txt --outfile a.txt --sys "傲嬌貓娘" --max-tokens 256
---   host.exe cli.lua --help
---
--- completion 印到 stdout；token 用量／錯誤印到 stderr。旗標名＝schema 名稱把底線換連字號。

local HERE = debug.getinfo(1, "S").source:sub(2):match("^(.*[/\\])") or ""
local llm = dofile(HERE .. "llm.lua")

-- 由 schema 生成 flag → entry 對照（--name-with-hyphens）；func 型參數（如 callback）CLI 無法帶，跳過
local function flag_of(name) return "--" .. (name:gsub("_", "-")) end
local by_flag = {}
for _, e in ipairs(llm.schema) do
  if e[3] ~= "func" then by_flag[flag_of(e[1])] = e end
end

local function usage()
  io.stderr:write("用法：host.exe cli.lua [--旗標 值 ...]\n可用旗標（由 llm.lua schema 生成）：\n")
  for _, e in ipairs(llm.schema) do
    if e[3] ~= "func" then
      local desc = (e[3] == "bool") and "(布林旗標，無值)"
          or ((e[2] == "ctrl") and "(控制)" or (e[2] .. " (" .. e[3] .. ")"))
      io.stderr:write(string.format("  %-14s %s\n", flag_of(e[1]), desc))
    end
  end
end

-- 依 schema 值型別把命令列字串轉成正確型別
local function coerce(e, raw)
  local t = e[3]
  if t == "str" then return raw end
  local num = tonumber(raw)
  if not num then error(flag_of(e[1]) .. " 需要數值，得到：" .. raw, 0) end
  if t == "int" then
    local i = math.tointeger(num)
    if not i then error(flag_of(e[1]) .. " 需要整數，得到：" .. raw, 0) end
    return i
  end
  return num  -- "num"
end

-- 解析 arg（host 綁的全域表；arg[1..] 是 cli.lua 後面的參數）
local opts, i = {}, 1
while i <= #arg do
  local flag = arg[i]
  if flag == "--help" or flag == "-h" then usage(); os.exit(0) end
  local e = by_flag[flag]
  if not e then
    io.stderr:write("未知旗標：" .. tostring(flag) .. "\n"); usage(); os.exit(1)
  end
  if e[3] == "bool" then                    -- 布林旗標：出現即 true，不吃下一個 argv
    opts[e[1]] = true
    i = i + 1
  else
    local raw = arg[i + 1]
    if raw == nil then io.stderr:write(flag .. " 缺少值\n"); os.exit(1) end
    local ok, val = pcall(coerce, e, raw)
    if not ok then io.stderr:write(tostring(val) .. "\n"); os.exit(1) end
    opts[e[1]] = val
    i = i + 2
  end
end

if next(opts) == nil then usage(); os.exit(1) end

-- --streaming：CLI 不能帶函式，注入內建 callback＝即時印到 stdout（flush 確保逐字出現）
if opts.streaming then
  opts.callback = function(chunk) io.write(chunk); io.flush() end
end

local ok, result = pcall(llm.llm_entry, opts)
if not ok then io.stderr:write(tostring(result) .. "\n"); os.exit(2) end

if opts.streaming then                  -- 串流：內容已由 callback 印出；result＝{ok=…}
  io.write("\n")
  os.exit((result and result.ok) and 0 or 2)
end
if result == nil then os.exit(2) end    -- 回應解析失敗，llm_entry 已印原始回應
if opts.outfile then
  io.stderr:write("[寫入] " .. (opts.lua and opts.outfile or result) .. "\n")
elseif opts.lua then
  io.write(llm.dump(result)); io.write("\n")   -- --lua：回原生 table，回顯成乾淨 Lua 字面量
else
  io.write(result); io.write("\n")
end
