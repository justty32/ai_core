-- main.lua — gen_v0（v1 引擎）示範：確定性＋全場搜尋＋intent 層。
-- 執行：cd gen_v0 && lua main.lua

package.path = "./?.lua;" .. package.path
local engine = require("engine")
local T      = require("tables")

local function base_conditions() return dofile("conditions.lua") end

local function hr(label)
  io.write(("\n========== %s ==========\n"):format(label))
end

local function show_trace(trace)
  io.write(("-- 座標：%s/%s/%s/%s｜相容性 %d＋intent %d｜文法變體 %s｜總成本 %d%s\n")
    :format(trace.coord.topic, trace.coord.give, trace.coord.gap, trace.coord.ending,
            trace.price, trace.icost, trace.variant, trace.total,
            trace.paid_violation and "｜⚠ 付費違反（✕格）" or ""))
  io.write(("-- 搜尋：試了 %d 個座標、可行 %d 個\n")
    :format(trace.search.tried, trace.search.feasible))
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
hr("① 基準：明給座標 A1/B1/C2（ending 由 D≈f(A) 派生）")
local cond = base_conditions()
local text1, trace1 = engine.generate(cond, T)
io.write(text1); show_trace(trace1)

------------------------------------------------------------------
hr("② 確定性自檢：同條件再跑一次，逐位元比對")
local text2 = engine.generate(base_conditions(), T)
assert(text1 == text2, "確定性破功！同條件出了不同稿")
io.write("同條件 → 逐位元相同 ✓（G 是純函數）\n")

------------------------------------------------------------------
hr("③ 歷史入條件：把①用過的句子登記進 history")
local cond3 = base_conditions()
for _, p in ipairs(trace1.picks) do
  cond3.history.used_lines[p.id] = true
end
local text3, trace3 = engine.generate(cond3, T)
io.write(text3); show_trace(trace3)
io.write("（同座標、但歷史不同 → 換到次優候選；骨架不變、字面全換）\n")

------------------------------------------------------------------
hr("④ intent 層：座標全留空、只說「要靜一點」→ 搜尋自己選座標")
local cond4 = base_conditions()
cond4.scene.coordinate = {}          -- 四軸全交給搜尋
cond4.scene.intent     = "靜"
local text4, trace4 = engine.generate(cond4, T)
io.write(text4); show_trace(trace4)
assert(trace4.coord.topic == "A2" and trace4.coord.ending == "T9",
  "intent=靜 應收斂到 A2（車票）×T9（定格）")

------------------------------------------------------------------
hr("⑤ intent 層：同樣全留空、改說「要甜一點」")
local cond5 = base_conditions()
cond5.scene.coordinate = {}
cond5.scene.intent     = "甜"
local text5, trace5 = engine.generate(cond5, T)
io.write(text5); show_trace(trace5)
assert(trace5.coord.topic == "A1" and trace5.coord.ending == "T10",
  "intent=甜 應收斂到 A1（讓書）×T10（鉤子）")

------------------------------------------------------------------
hr("⑥ 代價模型：硬點 A2×T10（✕格，定價 10）——量產預算 0 vs 劇本預算 12")
local cond6 = base_conditions()
cond6.scene.coordinate = { topic = "A2", give = "B3", gap = "C1", ending = "T10" }
local ok6, err6 = pcall(engine.generate, cond6, T)
assert(not ok6)
io.write("預算 0 → 被擋：" .. err6 .. "\n")
cond6.scene.narrative_budget = 12
local text6, trace6 = engine.generate(cond6, T)
io.write("預算 12 → 放行（好的違反＝付得起代價的違反）：\n\n")
io.write(text6); show_trace(trace6)

------------------------------------------------------------------
hr("⑦ 階門檻硬約束：階 1 想開 A5 後庫話題（需要階 2）")
local cond7 = base_conditions()
cond7.relation.stage = 1
cond7.scene.coordinate.topic = "A5"
local ok7, err7 = pcall(engine.generate, cond7, T)
assert(not ok7)
io.write("被 L1 擋下：" .. err7 .. "\n")

------------------------------------------------------------------
hr("⑧ 表版本也是條件：條件要求舊版表")
local cond8 = base_conditions()
cond8.version.tables = "2026-07-01x"
local ok8, err8 = pcall(engine.generate, cond8, T)
assert(not ok8)
io.write("被版本檢查擋下：" .. err8 .. "\n")

io.write("\n全部示範完成。\n")
