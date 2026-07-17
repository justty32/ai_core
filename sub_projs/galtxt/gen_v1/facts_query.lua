-- facts_query.lua — 讀取側五動詞：resolve / match / trace / backrefs / layer。
-- 讀處處可讀（寫入門在 store／lod／tx）；一切回傳恆為插入序（確定性）。

local U = require("facts_util")
local Q = {}

-- resolve：先過視角 overlay，再沿 finer 鏈下行到當下最細值（LOD 讀取時解析）
function Q.resolve(self, id, view)
  local vid, why = (view or self:view("全知")).map(id)
  if not vid then return nil, why end
  if U.is_placeholder(vid) then return nil, "佔位（尚未長出）" end
  local f = assert(self.by_id[vid], "未知事實：" .. tostring(vid))
  while f.finer do f = self.by_id[f.finer] end
  return f.value, f.id
end

-- match：圖模式查詢，pattern 欄位全等 AND；特例 {who=…, state="knows"/"hides"} 查知識視圖
function Q.match(self, pattern, view)
  if pattern.state then
    local k = self.knowledge[pattern.who] or {}
    local out = {}
    for _, id in ipairs(k[pattern.state] or {}) do out[#out + 1] = id end
    return out
  end
  local out = {}
  for _, f in ipairs(self.list) do
    local ok = true
    for key, v in pairs(pattern) do
      if f[key] ~= v then ok = false break end
    end
    if ok and (not view or view.map(f.id)) then out[#out + 1] = f.id end
  end
  return out
end

-- trace：沿段（優先）或 refs 下行展開；佔位標記出來（接地檢查、導因鏈展開）
function Q.trace(self, id, out, depth)
  out, depth = out or {}, depth or 0
  out[#out + 1] = { id = id, depth = depth, placeholder = U.is_placeholder(id) or nil }
  local f = self.by_id[id]
  if f then
    if f.segments then
      for _, s in ipairs(f.segments) do self:trace(s, out, depth + 1) end
    elseif f.kind == "連結" then
      for _, r in ipairs(f.refs) do self:trace(r, out, depth + 1) end
    end
  end
  return out
end

-- backrefs：誰指著我（refs 或 parent）——一致性傳播名單
function Q.backrefs(self, id)
  local out = {}
  for _, f in ipairs(self.list) do
    if (f.kind == "連結" and U.has(f.refs, id)) or f.parent == id then
      out[#out + 1] = f.id
    end
  end
  return out
end

-- layer：節點＝0（基石）；連結＝所引用事實的最大層＋1（佔位不計）
function Q.layer(self, id)
  local f = assert(self.by_id[id], "未知事實：" .. tostring(id))
  if f.kind == "節點" then return 0 end
  local top = 0
  for _, r in ipairs(f.refs) do
    if not U.is_placeholder(r) then
      local l = self:layer(r)
      if l > top then top = l end
    end
  end
  return top + 1
end

return Q
