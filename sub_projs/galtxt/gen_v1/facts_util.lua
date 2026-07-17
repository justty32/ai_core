-- facts_util.lua — facts 子模組共用小工具（無狀態純函式）。

local U = {}

function U.has(arr, v)
  for _, x in ipairs(arr) do if x == v then return true end end
  return false
end

function U.remove_val(arr, v)
  for i, x in ipairs(arr) do if x == v then table.remove(arr, i) return end end
end

-- 佔位連結：refs 裡「?」開頭＝連到還不存在的事實（懸空合法，柱四）
function U.is_placeholder(id)
  return type(id) == "string" and id:sub(1, 1) == "?"
end

-- 排斥邊檢查：有無「排斥」邊連著 id、而另一端已 canon？（柱四：兩端不得同真）
function U.exclusion_block(db, id)
  for _, f in ipairs(db.list) do
    if f.kind == "連結" and f.edge_type == "排斥" and U.has(f.refs, id) then
      for _, other in ipairs(f.refs) do
        if other ~= id and db.by_id[other] and db.by_id[other].canon then
          return other
        end
      end
    end
  end
  return nil
end

return U
