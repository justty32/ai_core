-- main.lua — gen_v0 示範：同條件同輸出、變化全上移到條件。
-- 執行：cd gen_v0 && lua main.lua

package.path = "./?.lua;" .. package.path
local engine = require("engine")
local T      = require("tables")

local function base_conditions() return dofile("conditions.lua") end

local function hr(label)
  io.write(("\n========== %s ==========\n"):format(label))
end

local function show_trace(trace)
  io.write(("-- 座標：%s/%s/%s/%s（相容性定價 %d）\n")
    :format(trace.coord.topic, trace.coord.give, trace.coord.gap, trace.coord.ending, trace.price))
  for _, p in ipairs(trace.picks) do
    io.write(("--   %-4s → %s（成本 %d）\n"):format(p.beat, p.id, p.cost))
  end
  local parts = {}
  for _, k in ipairs({ "kuchiguse", "question", "ellipsis", "leak" }) do
    parts[#parts + 1] = ("%s=%d"):format(k, trace.spent[k] or 0)
  end
  io.write("--   預算消耗：" .. table.concat(parts, "  ") .. "\n")
end

------------------------------------------------------------------
hr("① 基準：conditions.lua 原樣")
local cond = base_conditions()
local text1, trace1 = engine.generate(cond, T)
io.write(text1)
show_trace(trace1)

------------------------------------------------------------------
hr("② 確定性自檢：同條件再跑一次，逐位元比對")
local text2 = engine.generate(base_conditions(), T)
assert(text1 == text2, "確定性破功！同條件出了不同稿")
io.write("同條件 → 逐位元相同 ✓（G 是純函數）\n")

------------------------------------------------------------------
hr("③ 歷史入條件：把①用過的句子登記進 history，重複感的確定性解")
local cond3 = base_conditions()
for _, p in ipairs(trace1.picks) do
  cond3.history.used_lines[p.id] = true
end
local text3, trace3 = engine.generate(cond3, T)
io.write(text3)
show_trace(trace3)
io.write("（同座標、但歷史不同 → 全場自動換到次優候選；骨架不變、字面全換）\n")

------------------------------------------------------------------
hr("④ 變化上移到條件：只改 ending=T9（不再由 D≈f(A) 派生）")
local cond4 = base_conditions()
cond4.scene.coordinate.ending = "T9"
local text4, trace4 = engine.generate(cond4, T)
io.write(text4)
show_trace(trace4)

------------------------------------------------------------------
hr("⑤ 階門檻硬約束：階 1 想開 A5 後庫話題（需要階 2）")
local cond5 = base_conditions()
cond5.relation.stage = 1
cond5.scene.coordinate.topic = "A5"
local ok, err = pcall(engine.generate, cond5, T)
assert(not ok)
io.write("被 L1 擋下：" .. err .. "\n")

------------------------------------------------------------------
hr("⑥ 表版本也是條件：條件要求舊版表")
local cond6 = base_conditions()
cond6.version.tables = "2026-07-01x"
local ok6, err6 = pcall(engine.generate, cond6, T)
assert(not ok6)
io.write("被版本檢查擋下：" .. err6 .. "\n")

io.write("\n全部示範完成。\n")
