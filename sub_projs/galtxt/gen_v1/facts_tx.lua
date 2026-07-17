-- facts_tx.lua — 執行期寫入門：speculate 交易（begin→apply→check→commit/rollback）。
-- DFS 分支＝begin→apply→check→rollback；整條 act 序列定案才 commit。
-- act = { speaker=…, act=…, refs={id...}, effects={ reveal={id...}, to=角色 } }

local U = require("facts_util")
local T = {}

-- 呈現＝鎖 canon。投影含粗事實與構件：連結呈現則其 refs 也算呈現、細值呈現則父鏈也算。
local function present(db, id)
  if U.is_placeholder(id) then return end
  local f = assert(db.by_id[id])
  if f.canon then return end
  local blocker = U.exclusion_block(db, id)
  if blocker then
    error(("排斥邊擋下：%s 與已 canon 的 %s 不可同真"):format(id, blocker), 0)
  end
  f.canon = true
  if f.parent then present(db, f.parent) end
  if f.kind == "連結" then
    for _, r in ipairs(f.refs) do present(db, r) end
  end
end

function T.begin(self)
  local db = self
  local tx = { acts = {} }

  function tx.apply(act) tx.acts[#tx.acts + 1] = act end

  function tx.check()
    for _, act in ipairs(tx.acts) do
      local k = db.knowledge[act.speaker]   -- 旁白等無視圖者不受「知道才能說」約束
      for _, r in ipairs(act.refs or {}) do
        if U.is_placeholder(r) then return false, "引用佔位事實（需先細化）：" .. r end
        if not db.by_id[r] then return false, "引用不存在的事實：" .. r end
        if k and not (U.has(k.knows, r) or U.has(k.hides, r)) then
          return false, ("矛盾防線：%s 說出了自己不知道的事實 %s"):format(act.speaker, r)
        end
        local blocker = U.exclusion_block(db, r)
        if blocker and blocker ~= r then
          return false, ("排斥邊：%s 與已 canon 的 %s 不可同真"):format(r, blocker)
        end
      end
      for _, r in ipairs((act.effects or {}).reveal or {}) do
        if not (k and U.has(k.hides, r)) then
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
        U.remove_val(k.hides, r)
        if not U.has(k.knows, r) then k.knows[#k.knows + 1] = r end
        local lk = eff.to and db.knowledge[eff.to]
        if lk then
          if not U.has(lk.knows, r) then lk.knows[#lk.knows + 1] = r end
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

return T
