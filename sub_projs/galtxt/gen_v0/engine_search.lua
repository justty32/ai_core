-- engine_search.lua — G 的搜尋原語（L1 admissible／L2 產生式／L3+L4 全域填充）。
-- 純函數：無 RNG、無 IO、無時鐘。由 engine.lua 的 M.generate 編排調用。
-- 確定性三守則：只走 *_order 陣列與句庫排列序；嚴格 < 才更新最優（先枚舉者贏平手）；
--               歷史是輸入不是狀態。

local S = {}

local HISTORY_PENALTY = 100  -- 已用句的確定性懲罰（去重複感）
S.XCELL               = 10   -- ✕ 格定價（相容性梯度）

-- L1：候選是否吻合座標（requires 明列的軸都要對得上）
local function admissible(cand, coord)
  local r = cand.requires
  if not r then return true end
  for _, k in ipairs({ "topic", "give", "gap", "ending" }) do
    if r[k] ~= nil and r[k] ~= coord[k] then return false end
  end
  return true
end

------------------------------------------------------------------ L2：產生式展開
-- 回傳（確定性順序的）beat 序列變體清單，各帶結構成本
function S.expand_grammar(coord, G)
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
function S.fill_sequence(seq, coord, T, history)
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

return S
