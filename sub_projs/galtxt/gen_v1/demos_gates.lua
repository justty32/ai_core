-- demos_gates.lua — 示範⑤～⑨：執行期寫入門／洩底／排斥邊／speculate／約束細化。全數自帶 assert。

return function(db)

  ---------------------------------------------------------------- ⑤ 執行期寫入門：說不出不知道的事；呈現→canon 鎖
  local tx = db:begin()
  tx.apply{ speaker = "佐伯悠", act = "評書", refs = {"物#書417", "邊#她心動"} }
  local pass, why = tx.check()
  assert(not pass and why:find("矛盾防線", 1, true), "⑤ 他引用真相邊應被矛盾防線擋下")
  tx.rollback()

  tx = db:begin()
  tx.apply{ speaker = "佐伯悠", act = "評書", refs = {"物#書417", "邊#誤會"} }
  assert(tx.check())
  tx.commit()
  assert(db.by_id["物#書417"].canon and db.by_id["邊#誤會"].canon, "⑤ 呈現過即 canon")

  local ok = pcall(function() db:modify("物#書417", "普通文庫") end)
  assert(not ok, "⑤ canon 鎖：已呈現不可改")
  db:modify("態#書已售", "書#417 已售出（改：被人預訂）")   -- 未呈現可改，玩家無感
  print("⑤ 寫入門：矛盾防線＋canon 鎖（呈現才鎖、未呈現可改）　OK")

  ---------------------------------------------------------------- ⑥ 洩底＝知識轉移：hides→已表達、誤會被真相替換
  tx = db:begin()
  tx.apply{ speaker = "柏木秋穗", act = "洩底", refs = {"邊#她心動"},
            effects = { reveal = {"邊#她心動"}, to = "佐伯悠" } }
  assert(tx.check())
  tx.commit()

  assert(#db:match{ who = "柏木秋穗", state = "hides" } == 0,
    "⑥ 洩底後 hides 清空（恰一記帳可據此計數）")
  local belief = db:resolve("邊#她心動", db:view("佐伯悠"))
  assert(belief == "她對他心動", "⑥ 誤會拆除：他的視圖同一 ID 改解析到真相")
  assert(db.by_id["邊#她心動"].canon, "⑥ 洩了底＝呈現過＝canon")
  print("⑥ 洩底＝知識轉移＋誤會拆除　OK")

  ---------------------------------------------------------------- ⑦ 排斥邊：兩端不得同真
  tx = db:begin()
  tx.apply{ speaker = "旁白", act = "描寫", refs = {"態#書在架"} }
  assert(tx.check())
  tx.commit()

  tx = db:begin()
  tx.apply{ speaker = "旁白", act = "描寫", refs = {"態#書已售"} }
  pass, why = tx.check()
  assert(not pass and why:find("排斥邊", 1, true), "⑦ 在架已 canon，已售不得再呈現")
  tx.rollback()
  print("⑦ 排斥邊：矛盾檢查＝圖結構查詢　OK")

  ---------------------------------------------------------------- ⑧ speculate：試探分支不落盤；同查詢逐位元相同
  local snap1 = db:dump()
  tx = db:begin()
  tx.apply{ speaker = "旁白", act = "描寫", refs = {"場#河鹿堂"} }
  assert(tx.check())
  tx.rollback()                       -- DFS 退出分支
  assert(db:dump() == snap1, "⑧ rollback 後全庫快照逐位元相同")

  local m1 = table.concat(db:match{ kind = "連結" }, ";")
  local m2 = table.concat(db:match{ kind = "連結" }, ";")
  assert(m1 == m2 and m1 ~= "", "⑧ match 恆為插入序（確定性）")
  print("⑧ speculate 分支不落盤＋查詢確定性　OK")

  ---------------------------------------------------------------- ⑨ 約束細化：解不出→掛起工單（編譯期 LLM 崗位）
  local got, ticket = db:refine("場#校園後方", { grain = "場所級", constraints = { mood = "櫻花" } })
  assert(got == nil and ticket:find("掛起工單", 1, true), "⑨ 庫裡沒有滿足點→掛起")
  assert(#db.pending == 1)

  -- 編譯期補件（LLM 炸候選→人審→入庫的入庫那一步；分佈未鎖，世界為故事讓路）
  db:add{ id = "場#櫻花樹下", kind = "節點", parent = "場#校園後方",
          mood = "櫻花", value = "校園後方・櫻花樹下" }
  got = db:refine("場#校園後方", { grain = "場所級", constraints = { mood = "櫻花" } })
  assert(got == "場#櫻花樹下", "⑨ 補件後同一查詢解出告白級地點")
  print("⑨ 約束細化＋掛起工單（告白要在櫻花樹下）　OK")

end
