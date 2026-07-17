-- facts_store.lua — 編譯期寫入門①（add 入庫）＋ modify（canon 鎖）＋知識視圖＋dump。
-- 總則見 facts.lua：確定性遍歷走 list 插入序；不變式架在門上。

local U = require("facts_util")
local S = {}

------------------------------------------------------------------ 編譯期寫入門①：add（入庫）

-- 檢查：ID 唯一、連結必有 refs、refs 必存在（佔位豁免）→ 接地不變式由歸納保證
function S.add(self, f)
  assert(f.id and not self.by_id[f.id], "ID 缺失或重複：" .. tostring(f.id))
  assert(f.kind == "節點" or f.kind == "連結", "kind 必須是 節點/連結：" .. tostring(f.kind))
  if f.kind == "連結" then
    assert(type(f.refs) == "table" and #f.refs >= 1, "連結必須有 refs：" .. f.id)
    for _, r in ipairs(f.refs) do
      if not U.is_placeholder(r) and not self.by_id[r] then
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

------------------------------------------------------------------ modify（未呈現可改；canon 鎖）

function S.modify(self, id, value)
  local f = assert(self.by_id[id], "未知事實：" .. tostring(id))
  if f.canon then
    error("canon 鎖：已呈現的事實不可改（只可再細化）：" .. id, 0)
  end
  f.value = value
end

------------------------------------------------------------------ 知識視圖（柱二）

function S.set_knowledge(self, who, t)
  self.knowledge[who] = {
    knows = t.knows or {}, hides = t.hides or {}, variants = t.variants or {},
  }
end

-- 視角＝overlay：全知恆等；角色＝variants 先替換（誤會）、knows∪hides 可見、其餘不可見
function S.view(self, who)
  if who == "全知" then
    return { who = who, map = function(id) return id end }
  end
  local k = assert(self.knowledge[who], "沒有這個角色的知識視圖：" .. tostring(who))
  return { who = who, map = function(id)
    if k.variants[id] then return k.variants[id] end   -- 誤會：映到錯誤變體
    if U.has(k.knows, id) or U.has(k.hides, id) then return id end
    return nil, "不可見"
  end }
end

------------------------------------------------------------------ dump（測試用：全庫確定性快照）

function S.dump(self)
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

return S
