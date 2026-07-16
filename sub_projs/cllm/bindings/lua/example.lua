-- example.lua — cllm Lua binding 示範。
-- 跑法（build.sh 會設好 LUA_CPATH 找到 llm.so）：
--   lua example.lua "file:///<cllm絕對路徑>/test/fixtures/"   # 離線走 fixture
--   lua example.lua                                            # 無參數 → 內建 localhost（要真後端）
local llm = require("llm")

local base = arg[1] or ""  -- fixture 目錄或空（空＝用內建預設 endpoint）

-- ① 位置形式：prompt + endpoint（沒有 base 時 endpoint 為 ""，binding 當作未設 → 內建 localhost）
local ep = base ~= "" and (base .. "fake/chat/completions") or nil
local ans = ep and llm.ask("你好", ep) or llm.ask("你好")
print("[lua] llm.ask => " .. ans)

-- ② table 形式 + 串流 on_delta（逐段即時印；回 false 續、true 中止）
if base ~= "" then
  io.write("[lua] 串流逐段 => ")
  local whole = llm.ask{
    prompt   = "數到五",
    endpoint = base .. "fake_stream/chat/completions",
    stream   = true,
    on_delta = function(piece)
      io.write("[" .. piece .. "]")
      return false
    end,
  }
  print("　合＝" .. whole)
end
