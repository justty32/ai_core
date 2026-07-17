-- engine.lua — G(條件, 規則表) → 台詞。純函數：無 RNG、無 IO、無時鐘。
-- v1：整場戲當一個搜尋問題解全域最優（「單句不計延遲」的授權）。本檔＝編排本體：
--   L1 座標搜尋（本檔）：明給的軸鎖定、留空的軸枚舉；門檻/✕定價/intent 全進成本
--   L2/L3+L4 搜尋原語 → engine_search.lua｜組稿＋L5 護欄＋trace → engine_build.lua
-- 確定性三守則：只走 *_order 陣列與句庫排列序；嚴格 < 才更新最優（先枚舉者贏平手）；
--               歷史是輸入不是狀態。

local S   = require("engine_search")
local Bld = require("engine_build")
local M   = {}

function M.generate(cond, T)
  if cond.version.tables ~= T.version then
    error(("表版本不符：條件要 %s、載入的是 %s"):format(cond.version.tables, T.version), 0)
  end

  local naming = assert(T.naming[cond.relation.stage], "未知的稱謂階")
  local want   = cond.scene.coordinate or {}
  local intent = cond.scene.intent
  local budget = cond.scene.narrative_budget or 0
  local slots  = {
    her_full = cond.cast.heroine,
    his_call = naming.his_call,   -- 他怎麼叫她
    her_call = naming.her_call,   -- 她怎麼叫他
  }

  -- L1 搜尋空間：明給的軸＝單元素、留空的軸＝全枚舉（確定性順序）
  local function axis(explicit, order)
    if explicit then return { explicit } end
    return order
  end
  local topics  = axis(want.topic,  T.topic_order)
  local gives   = axis(want.give,   T.give_order)
  local gaps    = axis(want.gap,    T.gap_order)
  local endings = want.ending and { want.ending }
                or (intent and T.ending_order
                or { false })  -- false＝哨兵：稍後 D≈f(A) 派生（{nil} 是空表，不能用）

  local best = nil   -- { cost, coord, price, icost, variant, filled }
  local tried, feasible = 0, 0
  local rejects = {}  -- 拒絕理由統計（誠實覆蓋率：不靜默截斷）
  local function reject(reason) rejects[reason] = (rejects[reason] or 0) + 1 end

  for _, tp in ipairs(topics) do
    local topic = T.topics[tp]
    if not topic then error("未知話題：" .. tostring(tp), 0) end
    if cond.relation.stage < topic.gate then
      tried = tried + 1
      reject(("門檻（%s 需階 %d）"):format(topic.name, topic.gate))
    else
      for _, gv in ipairs(gives) do
        for _, gp in ipairs(gaps) do
          for _, ed in ipairs(endings) do
            local coord = { topic = tp, give = gv, gap = gp,
                            ending = (ed ~= false and ed) or T.ending_of_temp[topic.temp] }
            tried = tried + 1
            local ab = (T.compat.AB[tp] or {})[gv] or S.XCELL
            local ac = (T.compat.AC[tp] or {})[gp] or S.XCELL
            local ad = (T.compat.AD[tp] or {})[coord.ending] or S.XCELL
            local price = ab + ac + ad
            local has_x = (ab >= S.XCELL or ac >= S.XCELL or ad >= S.XCELL)
            if has_x and budget < price then
              reject("✕格買不起")
            else
              local icost = intent and T.intent_cost[intent](topic, coord) or 0
              -- L2～L4：對每個文法變體做全域填充，取整場最小
              local found = false
              for _, v in ipairs(S.expand_grammar(coord, T.grammar)) do
                local filled = S.fill_sequence(v.seq, coord, T, cond.history.used_lines)
                if filled then
                  found = true
                  local total = price + icost + v.cost + filled.cost
                  if best == nil or total < best.cost then  -- 嚴格 <：先枚舉者贏平手
                    best = { cost = total, coord = coord, price = price,
                             icost = icost, variant = v, filled = filled,
                             paid_violation = has_x }
                  end
                end
              end
              if found then feasible = feasible + 1
              else reject("句庫無可行組合") end
            end
          end
        end
      end
    end
  end

  if not best then Bld.no_solution_error(rejects) end
  return Bld.assemble(best, cond, T, slots, tried, feasible, rejects)
end

return M
