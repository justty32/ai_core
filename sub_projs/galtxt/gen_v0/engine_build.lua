-- engine_build.lua — G 的組稿層（L5 護欄前後）：槽填充、無解錯誤、成稿＋trace。
-- 純函數。由 engine.lua 的 M.generate 在搜出 best 之後調用。

local B = {}

-- 槽填充：${slot} → slots[slot]；未定義即報錯（不靜默）
local function fill(text, slots)
  return (text:gsub("%$%{([%w_]+)%}", function(k)
    return slots[k] or error(("句庫引用了未定義的槽：${%s}"):format(k))
  end))
end

-- 整個搜尋空間無解：把拒絕理由統計排序成確定性錯誤訊息後拋出
function B.no_solution_error(rejects)
  local keys = {}
  for k in pairs(rejects) do keys[#keys + 1] = k end
  table.sort(keys)  -- 排序後再組字串：錯誤訊息也要確定性
  local parts = {}
  for _, k in ipairs(keys) do parts[#parts + 1] = ("%s×%d"):format(k, rejects[k]) end
  error("整個搜尋空間無解：" .. table.concat(parts, "；"), 0)
end

-- 成稿：組稿 → L5 護欄 → 回傳 text 與可解釋 trace
function B.assemble(best, cond, T, slots, tried, feasible, rejects)
  local blocks = {}
  for _, p in ipairs(best.filled.picks) do
    blocks[#blocks + 1] = fill(p.cand.text, slots)
  end
  local title = ("【%s%s・%s】"):format(cond.world.season, cond.world.time, cond.world.place)
  local text = title .. "\n\n" .. table.concat(blocks, "\n\n") .. "\n"

  -- L5 護欄：命中即拒絕整段輸出
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
    coord = best.coord, price = best.price, intent = cond.scene.intent, icost = best.icost,
    variant = best.variant.label, total = best.cost,
    paid_violation = best.paid_violation,
    search = { tried = tried, feasible = feasible, rejects = rejects },
    picks = best.filled.picks, spent = spent,
  }
  return text, trace
end

return B
