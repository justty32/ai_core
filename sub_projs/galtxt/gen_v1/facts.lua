-- facts.lua — 事實庫：帶型別邊的分層 LOD 網（設計見 00_設計.md 柱二/三/四＋查詢介面）。
-- 玩具版鐵則同 gen_v0：純 Lua、無 RNG、無 IO、無時鐘；一切遍歷走 list 插入序（確定性）。
-- 讀寫不對稱：讀處處可讀；寫只有兩道門——
--   編譯期門：add（入庫）＋ refine（細化；約束解不出→掛起工單）
--   執行期門：act 經 speculate 交易 commit（此時才鎖 canon、動知識視圖）
-- 不變式全架在門上：接地、父約束、段鏈首尾、排斥邊、canon 鎖、「說不出不知道的事」。
-- 識別子英文、值與 ID 可中文（同 gen_v0 慣例）。

local M = {}
M.__index = M

function M.new()
  return setmetatable({
    list      = {},  -- 事實陣列：插入序＝唯一合法遍歷序（確定性；禁 hash 序）
    by_id     = {},
    knowledge = {},  -- [角色] = { knows={id...}, hides={id...}, variants={[真相id]=變體id} }
    pending   = {},  -- 掛起的編譯期工單（refine 解不出時排隊，等 LLM 炸候選→人審）
  }, M)
end

------------------------------------------------------------------ 小工具

local function has(arr, v)
  for _, x in ipairs(arr) do if x == v then return true end end
  return false
end

local function remove_val(arr, v)
  for i, x in ipairs(arr) do if x == v then table.remove(arr, i) return end end
end

-- 佔位連結：refs 裡「?」開頭＝連到還不存在的事實（懸空合法，柱四）
local function is_placeholder(id)
  return type(id) == "string" and id:sub(1, 1) == "?"
end

-- 排斥邊檢查：有無「排斥」邊連著 id、而另一端已 canon？（柱四：兩端不得同真）
local function exclusion_block(db, id)
  for _, f in ipairs(db.list) do
    if f.kind == "連結" and f.edge_type == "排斥" and has(f.refs, id) then
      for _, other in ipairs(f.refs) do
        if other ~= id and db.by_id[other] and db.by_id[other].canon then
          return other
        end
      end
    end
  end
  return nil
end

------------------------------------------------------------------ 編譯期寫入門①：add（入庫）

-- 檢查：ID 唯一、連結必有 refs、refs 必存在（佔位豁免）→ 接地不變式由歸納保證
function M:add(f)
  assert(f.id and not self.by_id[f.id], "ID 缺失或重複：" .. tostring(f.id))
  assert(f.kind == "節點" or f.kind == "連結", "kind 必須是 節點/連結：" .. tostring(f.kind))
  if f.kind == "連結" then
    assert(type(f.refs) == "table" and #f.refs >= 1, "連結必須有 refs：" .. f.id)
    for _, r in ipairs(f.refs) do
      if not is_placeholder(r) and not self.by_id[r] then
        error("接地失敗：refs 指向不存在的事實 " .. tostring(r) .. "（佔位請用 ? 前綴）", 0)
      end
    end
  end
  if f.parent then
    assert(self.by_id[f.parent], "parent 指向不存在的事實：" .. tostring(f.parent))
  end
  f.canon = f.canon or false
  self.list[#self.list + 1] = f
  self.by_id[f.id] = f
  return f.id
end

------------------------------------------------------------------ 編譯期寫入門②：refine（細化）

-- 三種形態：
--   節點取點   spec={ value=…, id=… }        → 父約束（range）內取點，父.finer 指向細層
--   連結加段   spec={ segments={連結id...} } → 鏈首尾＝粗邊首尾、段段相接（不推翻粗邊語義）
--   約束滿足   spec={ constraints={k=v…} }   → 在既有子事實裡找滿足點；找不到→掛起工單
function M:refine(parent_id, spec)
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

------------------------------------------------------------------ modify（未呈現可改；canon 鎖）

function M:modify(id, value)
  local f = assert(self.by_id[id], "未知事實：" .. tostring(id))
  if f.canon then
    error("canon 鎖：已呈現的事實不可改（只可再細化）：" .. id, 0)
  end
  f.value = value
end

------------------------------------------------------------------ 知識視圖（柱二）

function M:set_knowledge(who, t)
  self.knowledge[who] = {
    knows = t.knows or {}, hides = t.hides or {}, variants = t.variants or {},
  }
end

-- 視角＝overlay：全知恆等；角色＝variants 先替換（誤會）、knows∪hides 可見、其餘不可見
function M:view(who)
  if who == "全知" then
    return { who = who, map = function(id) return id end }
  end
  local k = assert(self.knowledge[who], "沒有這個角色的知識視圖：" .. tostring(who))
  return { who = who, map = function(id)
    if k.variants[id] then return k.variants[id] end   -- 誤會：映到錯誤變體
    if has(k.knows, id) or has(k.hides, id) then return id end
    return nil, "不可見"
  end }
end

------------------------------------------------------------------ 六動詞・讀取側

-- resolve：先過視角 overlay，再沿 finer 鏈下行到當下最細值（LOD 讀取時解析）
function M:resolve(id, view)
  local vid, why = (view or self:view("全知")).map(id)
  if not vid then return nil, why end
  if is_placeholder(vid) then return nil, "佔位（尚未長出）" end
  local f = assert(self.by_id[vid], "未知事實：" .. tostring(vid))
  while f.finer do f = self.by_id[f.finer] end
  return f.value, f.id
end

-- match：圖模式查詢，pattern 欄位全等 AND；特例 {who=…, state="knows"/"hides"} 查知識視圖。
-- 回傳恆為插入序（確定性）。
function M:match(pattern, view)
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
function M:trace(id, out, depth)
  out, depth = out or {}, depth or 0
  out[#out + 1] = { id = id, depth = depth, placeholder = is_placeholder(id) or nil }
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
function M:backrefs(id)
  local out = {}
  for _, f in ipairs(self.list) do
    if (f.kind == "連結" and has(f.refs, id)) or f.parent == id then
      out[#out + 1] = f.id
    end
  end
  return out
end

-- layer：節點＝0（基石）；連結＝所引用事實的最大層＋1（佔位不計）
function M:layer(id)
  local f = assert(self.by_id[id], "未知事實：" .. tostring(id))
  if f.kind == "節點" then return 0 end
  local top = 0
  for _, r in ipairs(f.refs) do
    if not is_placeholder(r) then
      local l = self:layer(r)
      if l > top then top = l end
    end
  end
  return top + 1
end

------------------------------------------------------------------ 執行期寫入門：speculate 交易

-- 呈現＝鎖 canon。投影含粗事實與構件：連結呈現則其 refs 也算呈現、細值呈現則父鏈也算。
local function present(db, id)
  if is_placeholder(id) then return end
  local f = assert(db.by_id[id])
  if f.canon then return end
  local blocker = exclusion_block(db, id)
  if blocker then
    error(("排斥邊擋下：%s 與已 canon 的 %s 不可同真"):format(id, blocker), 0)
  end
  f.canon = true
  if f.parent then present(db, f.parent) end
  if f.kind == "連結" then
    for _, r in ipairs(f.refs) do present(db, r) end
  end
end

-- DFS 分支＝begin→apply→check→rollback；整條 act 序列定案才 commit。
-- act = { speaker=…, act=…, refs={id...}, effects={ reveal={id...}, to=角色 } }
function M:begin()
  local db = self
  local tx = { acts = {} }

  function tx.apply(act) tx.acts[#tx.acts + 1] = act end

  function tx.check()
    for _, act in ipairs(tx.acts) do
      local k = db.knowledge[act.speaker]   -- 旁白等無視圖者不受「知道才能說」約束
      for _, r in ipairs(act.refs or {}) do
        if is_placeholder(r) then return false, "引用佔位事實（需先細化）：" .. r end
        if not db.by_id[r] then return false, "引用不存在的事實：" .. r end
        if k and not (has(k.knows, r) or has(k.hides, r)) then
          return false, ("矛盾防線：%s 說出了自己不知道的事實 %s"):format(act.speaker, r)
        end
        local blocker = exclusion_block(db, r)
        if blocker and blocker ~= r then
          return false, ("排斥邊：%s 與已 canon 的 %s 不可同真"):format(r, blocker)
        end
      end
      for _, r in ipairs((act.effects or {}).reveal or {}) do
        if not (k and has(k.hides, r)) then
          return false, ("洩底無效：%s 沒有藏著 %s"):format(act.speaker, r)
        end
      end
    end
    return true
  end

  function tx.commit()
    local ok, why = tx.check()
    if not ok then error("寫入門拒絕：" .. why, 0) end
    for _, act in ipairs(tx.acts) do
      for _, r in ipairs(act.refs or {}) do present(db, r) end
      local eff = act.effects or {}
      local k = db.knowledge[act.speaker]
      for _, r in ipairs(eff.reveal or {}) do
        -- 洩底＝知識轉移：hides→已表達（自己 knows）、聽者 knows 增加、誤會變體被真相替換
        remove_val(k.hides, r)
        if not has(k.knows, r) then k.knows[#k.knows + 1] = r end
        local lk = eff.to and db.knowledge[eff.to]
        if lk then
          if not has(lk.knows, r) then lk.knows[#lk.knows + 1] = r end
          lk.variants[r] = nil
        end
        present(db, r)
      end
    end
    tx.acts = {}
  end

  function tx.rollback() tx.acts = {} end

  return tx
end

------------------------------------------------------------------ dump（測試用：全庫確定性快照）

function M:dump()
  local parts = {}
  for _, f in ipairs(self.list) do
    parts[#parts + 1] = ("%s|%s|%s|%s"):format(
      f.id, tostring(f.value), tostring(f.canon), tostring(f.finer))
  end
  local names = {}
  for who in pairs(self.knowledge) do names[#names + 1] = who end
  table.sort(names)   -- 排序後再輸出：快照也要確定性
  for _, who in ipairs(names) do
    local k = self.knowledge[who]
    parts[#parts + 1] = who .. "|" .. table.concat(k.knows, ",")
                            .. "|" .. table.concat(k.hides, ",")
  end
  return table.concat(parts, "\n")
end

return M
