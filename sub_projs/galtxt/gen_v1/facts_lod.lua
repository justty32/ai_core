-- facts_lod.lua — 編譯期寫入門②：refine（細化）。柱三＋柱四的 LOD 紀律在此把關。
-- 三種形態：
--   節點取點   spec={ value=…, id=… }        → 父約束（range）內取點，父.finer 指向細層
--   連結加段   spec={ segments={連結id...} } → 鏈首尾＝粗邊首尾、段段相接（不推翻粗邊語義）
--   約束滿足   spec={ constraints={k=v…} }   → 在既有子事實裡找滿足點；找不到→掛起工單

local L = {}

function L.refine(self, parent_id, spec)
  local p = assert(self.by_id[parent_id], "未知事實：" .. tostring(parent_id))

  if spec.segments then
    assert(p.kind == "連結", "只有連結能加段：" .. parent_id)
    local segs = spec.segments
    local first, last = self.by_id[segs[1]], self.by_id[segs[#segs]]
    if first.refs[1] ~= p.refs[1] or last.refs[#last.refs] ~= p.refs[#p.refs] then
      error("段鏈首尾必須等於粗邊首尾（多段不得推翻粗連結語義）：" .. parent_id, 0)
    end
    for i = 1, #segs - 1 do
      local a, b = self.by_id[segs[i]], self.by_id[segs[i + 1]]
      if a.refs[#a.refs] ~= b.refs[1] then
        error("段鏈不連續：" .. segs[i] .. " → " .. segs[i + 1], 0)
      end
    end
    p.segments = segs
    return parent_id
  end

  if spec.constraints then
    for _, f in ipairs(self.list) do
      local ok = (f.parent == parent_id)
      if ok then
        for k, v in pairs(spec.constraints) do
          if f[k] ~= v then ok = false break end
        end
      end
      if ok then return f.id end   -- 插入序首個滿足者（確定性）
    end
    self.pending[#self.pending + 1] =
      { parent = parent_id, constraints = spec.constraints, grain = spec.grain }
    return nil, "掛起工單#" .. #self.pending
  end

  -- 節點取點
  if p.finer then
    error("已有細層：" .. p.finer .. "（要更細請對細層再 refine）", 0)
  end
  if p.range then
    if type(spec.value) ~= "number"
       or spec.value < p.range[1] or spec.value > p.range[2] then
      error(("細化只能落在父約束內：%s ∉ [%d,%d]")
        :format(tostring(spec.value), p.range[1], p.range[2]), 0)
    end
  end
  local id = assert(spec.id, "節點取點需給 id")
  self:add{ id = id, kind = p.kind, parent = parent_id, value = spec.value }
  p.finer = id   -- canon 事實也可細化（canon 鎖只擋 modify）
  return id
end

return L
