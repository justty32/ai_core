-- engine.lua — G(條件, 規則表) → 台詞。純函數：無 RNG、無 IO、無時鐘。
-- 五層：L1 座標(驗門檻+派生+定價) → L2 beat 展開 → L3 句規劃(預算簿記)
--       → L4 表層實現(選候選+填槽) → L5 檢核(護欄+審計)。
-- 確定性三守則：只用陣列順序遍歷；同分用 id 字典序 tie-break；歷史是輸入不是狀態。

local M = {}

local HISTORY_PENALTY = 100  -- 已用句的確定性懲罰（去重複感）

-- 候選是否符合本場座標（requires 缺省＝不限）
local function admissible(cand, coord)
  local r = cand.requires
  if not r then return true end
  for _, k in ipairs({ "topic", "give", "gap", "ending" }) do
    if r[k] ~= nil and r[k] ~= coord[k] then return false end
  end
  return true
end

-- 預算檢查：這個候選加進來會不會爆某項上限
local function within_budget(cand, spent, budgets)
  for k, v in pairs(cand.features or {}) do
    local cap = budgets[k]
    if cap and (spent[k] or 0) + v > cap then return false, k end
  end
  return true
end

local function charge(cand, spent)
  for k, v in pairs(cand.features or {}) do
    spent[k] = (spent[k] or 0) + v
  end
end

-- 填槽：${slot} ← 由稱謂階表/卡司決定，不許句庫自帶字面稱呼
local function fill(text, slots)
  return (text:gsub("%$%{([%w_]+)%}", function(k)
    return slots[k] or error(("句庫引用了未定義的槽：${%s}"):format(k))
  end))
end

-- L4 選型：可行集內取 (成本, id) 最小者——成本同分時 id 字典序保證全序
local function pick(cands, coord, spent, budgets, history)
  local best, best_cost, audit = nil, nil, {}
  for _, c in ipairs(cands) do
    local reason = nil
    if not admissible(c, coord) then
      reason = "座標不符"
    else
      local ok, over = within_budget(c, spent, budgets)
      if not ok then reason = "預算將爆：" .. over end
    end
    if reason then
      audit[#audit + 1] = { id = c.id, verdict = "排除（" .. reason .. "）" }
    else
      local cost = c.prior + (history[c.id] and HISTORY_PENALTY or 0)
      audit[#audit + 1] = { id = c.id, verdict = "候選", cost = cost }
      if best == nil or cost < best_cost
         or (cost == best_cost and c.id < best.id) then
        best, best_cost = c, cost
      end
    end
  end
  return best, best_cost, audit
end

function M.generate(cond, T)
  assert(cond.version.tables == T.version,
    ("表版本不符：條件要 %s、載入的是 %s"):format(cond.version.tables, T.version))

  ---------------------------------------------------------------- L1 座標
  local naming = assert(T.naming[cond.relation.stage], "未知的稱謂階")
  local want = cond.scene.coordinate
  local topic = assert(T.topics[want.topic], "未知話題：" .. tostring(want.topic))

  if cond.relation.stage < topic.gate then
    error(("階門檻擋下：話題「%s」需要階 %d，當前階 %d")
      :format(topic.name, topic.gate, cond.relation.stage), 0)
  end

  local coord = {
    topic  = want.topic,
    give   = want.give,
    gap    = want.gap,
    ending = want.ending or T.ending_of_temp[topic.temp],  -- D ≈ f(A)
  }

  -- 相容性定價（✓0 △2 ✕10）；超出敘事預算＝量產模式硬擋
  local price = (T.compat.AB[coord.topic][coord.give] or 10)
              + (T.compat.AC[coord.topic][coord.gap] or 10)
              + (T.compat.AD[coord.topic][coord.ending] or 10)
  if price > cond.scene.narrative_budget then
    if price >= 10 then
      error(("座標含 ✕ 格（定價 %d > 預算 %d），量產模式拒絕")
        :format(price, cond.scene.narrative_budget), 0)
    end
    -- △ 累計超預算：v0 從寬放行但記入 trace（劇本/量產的鬆緊之別）
  end

  ---------------------------------------------------------------- L2 beat
  local beats = {}
  for _, b in ipairs(T.beats) do
    local guard = T.beat_condition[b]
    if guard == nil or guard(coord) then beats[#beats + 1] = b end
  end

  ---------------------------------------------------------------- L3+L4
  local slots = {
    her_full = cond.cast.heroine,
    his_call = naming.his_call,   -- 他怎麼叫她
    her_call = naming.her_call,   -- 她怎麼叫他
  }
  local spent, blocks, trace = {}, {}, { coord = coord, price = price, picks = {} }

  for _, b in ipairs(beats) do
    local cands = assert(T.library[b], "句庫缺 beat：" .. b)
    local best, cost, audit = pick(cands, coord, spent, T.budgets, cond.history.used_lines)
    if not best then
      error(("beat %s 無可行候選（座標 %s/%s/%s/%s）")
        :format(b, coord.topic, coord.give, coord.gap, coord.ending), 0)
    end
    charge(best, spent)
    blocks[#blocks + 1] = fill(best.text, slots)
    trace.picks[#trace.picks + 1] = { beat = b, id = best.id, cost = cost, audit = audit }
  end

  ---------------------------------------------------------------- 組稿
  local title = ("【%s%s・%s】"):format(cond.world.season, cond.world.time, cond.world.place)
  local text = title .. "\n\n" .. table.concat(blocks, "\n\n") .. "\n"

  ---------------------------------------------------------------- L5 檢核
  for _, pat in ipairs(T.guards.forbidden) do
    if text:find(pat, 1, true) then
      error("護欄命中，拒絕輸出：" .. pat, 0)
    end
  end
  trace.spent = spent
  return text, trace
end

return M
