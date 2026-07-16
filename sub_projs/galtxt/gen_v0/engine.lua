-- engine.lua — G(條件, 規則表) → 台詞。純函數：無 RNG、無 IO、無時鐘。
-- v1：整場戲當一個搜尋問題解全域最優（「單句不計延遲」的授權，開始用了）。
--   L1 座標搜尋：明給的軸鎖定、留空的軸枚舉；門檻/✕定價/intent 全進成本
--   L2 beat 產生式：場景 := B0 B1 B2 B3+ (B4) B5 (B6) B7，依 c2/c3/c4 展開變體
--   L3+L4 全場窮舉：跨拍組合一起算（預算是全場約束，貪心會漏最優）
--   L5 護欄＋審計
-- 確定性三守則：只走 *_order 陣列與句庫排列序；嚴格 < 才更新最優（先枚舉者贏平手）；
--               歷史是輸入不是狀態。

local M = {}

local HISTORY_PENALTY = 100  -- 已用句的確定性懲罰（去重複感）
local XCELL           = 10   -- ✕ 格定價（相容性梯度）

------------------------------------------------------------------ 小工具

local function admissible(cand, coord)
  local r = cand.requires
  if not r then return true end
  for _, k in ipairs({ "topic", "give", "gap", "ending" }) do
    if r[k] ~= nil and r[k] ~= coord[k] then return false end
  end
  return true
end

local function fill(text, slots)
  return (text:gsub("%$%{([%w_]+)%}", function(k)
    return slots[k] or error(("句庫引用了未定義的槽：${%s}"):format(k))
  end))
end

------------------------------------------------------------------ L2：產生式展開
-- 回傳（確定性順序的）beat 序列變體清單，各帶結構成本
local function expand_grammar(coord, G)
  local bought = G.bought(coord)
  local mid = (coord.give == "B3") and "B5" or "B4B5"   -- c4：無讓渡→反差獨立拍
  local variants = {}
  for _, rounds in ipairs(G.rounds_order) do
    local b6_opts = bought and { "after", "before", "none" } or { "none" }  -- c2/c3
    for _, b6 in ipairs(b6_opts) do
      local seq = { "B0", "B1", "B2", "B3a" }
      if rounds == 2 then seq[#seq + 1] = "B3b" end
      if b6 == "before" then seq[#seq + 1] = "B6" end   -- c3：標點拍可在反差前
      seq[#seq + 1] = mid                                -- c1：反差必在話題後
      if b6 == "after" then seq[#seq + 1] = "B6" end
      seq[#seq + 1] = "B7"
      local cost = (rounds == 2 and G.round2_reward or 0)
                 + ((bought and b6 == "none") and G.b6_skip_cost or 0)
      variants[#variants + 1] = {
        seq = seq, cost = cost,
        label = ("%d輪·B6%s"):format(rounds, b6 == "none" and "無" or b6),
      }
    end
  end
  return variants
end

------------------------------------------------------------------ L3+L4：整段序列的全域最優填充
-- 對一條 beat 序列窮舉所有候選組合（帶預算 threading），回傳最優
local function fill_sequence(seq, coord, T, history)
  local best = nil  -- { cost, picks }
  local picks = {}

  -- 承接約束（requires_pick）：本候選要求「前面某拍選了特定句」——句際連貫性的硬約束
  local function setup_ok(c, upto)
    if not c.requires_pick then return true end
    for beat, id in pairs(c.requires_pick) do
      local found = nil
      for j = 1, upto do
        if picks[j] and picks[j].beat == beat then found = picks[j].id end
      end
      if found ~= id then return false end
    end
    return true
  end

  local function dfs(i, spent, acc)
    if best and acc >= best.cost then return end  -- 下界剪枝（成本非負增量）
    if i > #seq then
      best = { cost = acc, picks = { table.unpack(picks) } }
      return
    end
    local beat = seq[i]
    for _, c in ipairs(T.library[beat] or {}) do
      if admissible(c, coord) and setup_ok(c, i - 1) then
        -- 預算檢查＋記帳（全場約束，所以要 threading 進 DFS）
        local ok, spent2 = true, {}
        for k, v in pairs(spent) do spent2[k] = v end
        for k, v in pairs(c.features or {}) do
          local cap = T.budgets[k]
          spent2[k] = (spent2[k] or 0) + v
          if cap and spent2[k] > cap then ok = false break end
        end
        if ok then
          local cost = c.prior + (history[c.id] and HISTORY_PENALTY or 0)
          picks[i] = { beat = beat, id = c.id, cost = cost, cand = c }
          dfs(i + 1, spent2, acc + cost)
          picks[i] = nil
        end
      end
    end
  end

  dfs(1, {}, 0)
  return best
end

------------------------------------------------------------------ G 本體

function M.generate(cond, T)
  assert(cond.version.tables == T.version,
    ("表版本不符：條件要 %s、載入的是 %s"):format(cond.version.tables, T.version))

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
            local ab = (T.compat.AB[tp] or {})[gv] or XCELL
            local ac = (T.compat.AC[tp] or {})[gp] or XCELL
            local ad = (T.compat.AD[tp] or {})[coord.ending] or XCELL
            local price = ab + ac + ad
            local has_x = (ab >= XCELL or ac >= XCELL or ad >= XCELL)
            if has_x and budget < price then
              reject("✕格買不起")
            else
              local icost = intent and T.intent_cost[intent](topic, coord) or 0
              -- L2～L4：對每個文法變體做全域填充，取整場最小
              local found = false
              for _, v in ipairs(expand_grammar(coord, T.grammar)) do
                local filled = fill_sequence(v.seq, coord, T, cond.history.used_lines)
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

  if not best then
    local keys = {}
    for k in pairs(rejects) do keys[#keys + 1] = k end
    table.sort(keys)  -- 排序後再組字串：錯誤訊息也要確定性
    local parts = {}
    for _, k in ipairs(keys) do parts[#parts + 1] = ("%s×%d"):format(k, rejects[k]) end
    error("整個搜尋空間無解：" .. table.concat(parts, "；"), 0)
  end

  -- 組稿
  local blocks = {}
  for _, p in ipairs(best.filled.picks) do
    blocks[#blocks + 1] = fill(p.cand.text, slots)
  end
  local title = ("【%s%s・%s】"):format(cond.world.season, cond.world.time, cond.world.place)
  local text = title .. "\n\n" .. table.concat(blocks, "\n\n") .. "\n"

  -- L5 護欄
  for _, pat in ipairs(T.guards.forbidden) do
    if text:find(pat, 1, true) then
      error("護欄命中，拒絕輸出：" .. pat, 0)
    end
  end

  -- trace（可解釋性：選了什麼、為什麼、淘汰了多少）
  local spent = {}
  for _, p in ipairs(best.filled.picks) do
    for k, v in pairs(p.cand.features or {}) do spent[k] = (spent[k] or 0) + v end
  end
  local trace = {
    coord = best.coord, price = best.price, intent = intent, icost = best.icost,
    variant = best.variant.label, total = best.cost,
    paid_violation = best.paid_violation,
    search = { tried = tried, feasible = feasible, rejects = rejects },
    picks = best.filled.picks, spent = spent,
  }
  return text, trace
end

return M
