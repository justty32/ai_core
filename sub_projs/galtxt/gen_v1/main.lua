-- main.lua — gen_v1 事實庫九示範，各釘一條設計定調（00_設計.md）。全數自帶 assert。
-- 素材沿用河鹿堂定點：柏木秋穗 × 佐伯悠。

local F = require("facts")
local db = F.new()

------------------------------------------------------------------ 建材期：入庫（編譯期門①）

-- 第 0 層：節點（基石）
db:add{ id = "人#柏木秋穗", kind = "節點", value = "柏木秋穗" }
db:add{ id = "人#佐伯悠",   kind = "節點", value = "佐伯悠" }
db:add{ id = "物#書417",    kind = "節點", value = "推理文庫・絕版短篇集" }
db:add{ id = "場#河鹿堂",   kind = "節點", value = "舊書店・河鹿堂" }
db:add{ id = "場#校園後方", kind = "節點", value = "校園後方（粗）" }
db:add{ id = "時#年代",     kind = "節點", value = "90年代", form = "區間", range = {1990, 1999} }
db:add{ id = "事#圖書館",   kind = "節點", value = "去年在圖書館他替她解了圍" }
db:add{ id = "態#書在架",   kind = "節點", value = "書#417 還在店內書架" }
db:add{ id = "態#書已售",   kind = "節點", value = "書#417 已售出" }

-- 第 1 層：連結（連結也是事實：有 ID、有值、可入知識視圖）
db:add{ id = "邊#她心動", kind = "連結", edge_type = "心動",
        refs = {"人#柏木秋穗", "人#佐伯悠"}, value = "她對他心動" }
db:add{ id = "邊#誤會",   kind = "連結", edge_type = "嫌惡",
        refs = {"人#柏木秋穗", "人#佐伯悠"}, value = "她討厭他（他的誤會）" }
db:add{ id = "邊#常客",   kind = "連結", edge_type = "常客",
        refs = {"人#佐伯悠", "場#河鹿堂"}, value = "他是河鹿堂常客" }
db:add{ id = "邊#排斥書態", kind = "連結", edge_type = "排斥",
        refs = {"態#書在架", "態#書已售"} }

-- 第 2 層：連結的連結（戲劇結構住高層：真相與誤會的張力）
db:add{ id = "邊#認知落差", kind = "連結", edge_type = "張力",
        refs = {"邊#她心動", "邊#誤會"}, value = "真相與誤會的落差＝本線戲劇引擎" }

-- 知識視圖（柱二）：她藏著心動；他知道的那條是誤會變體
db:set_knowledge("柏木秋穗", {
  knows = {"人#柏木秋穗", "人#佐伯悠", "物#書417", "場#河鹿堂", "事#圖書館", "態#書在架"},
  hides = {"邊#她心動"},
})
db:set_knowledge("佐伯悠", {
  knows = {"人#柏木秋穗", "人#佐伯悠", "物#書417", "場#河鹿堂", "事#圖書館", "邊#誤會", "態#書在架"},
  variants = { ["邊#她心動"] = "邊#誤會" },   -- 誤會＝視圖裡映到錯誤變體
})

------------------------------------------------------------------ ① 分層網：節點 0、連結 1、連結的連結 2；接地不變式

assert(db:layer("人#柏木秋穗") == 0)
assert(db:layer("邊#她心動") == 1)
assert(db:layer("邊#認知落差") == 2)

local ok = pcall(function()
  db:add{ id = "邊#壞", kind = "連結", edge_type = "導因於", refs = {"邊#她心動", "事#不存在"} }
end)
assert(not ok, "① refs 指向不存在事實應被接地檢查擋下")

-- 懸空連結合法：佔位「?」＝backstory 需求記號（為什麼心動？之後再長）
db:add{ id = "邊#心動導因", kind = "連結", edge_type = "導因於",
        refs = {"邊#她心動", "?事#心動的更早源頭"} }
local tr = db:trace("邊#心動導因")
assert(tr[#tr].placeholder, "① 佔位事實應在 trace 中被標記")
print("① 分層網＋接地不變式＋懸空佔位　OK")

------------------------------------------------------------------ ② 節點 LOD：粗＝約束區間、細化＝區間內取點

local v = db:resolve("時#年代")
assert(v == "90年代", "② 未細化時解析到粗值")

db:refine("時#年代", { value = 1996, id = "時#1996" })
v = db:resolve("時#年代")
assert(v == 1996, "② 細化後同一 ID 解析到細值（連結指向粗事實拿當下最細 canon 值）")

ok = pcall(function() db:refine("時#1996", { value = 2005, id = "時#壞" }) end)
-- 時#1996 無 range，改測父：對已有細層的父再取點也該擋
ok = pcall(function() db:refine("時#年代", { value = 2005, id = "時#壞" }) end)
assert(not ok, "② 已有細層／出區間的細化應被擋（此處命中「已有細層」）")
print("② 節點 LOD：父約束內取點、resolve 拿最細值　OK")

------------------------------------------------------------------ ③ 連結 LOD：加中繼段（多段不推翻粗邊語義）

db:add{ id = "邊#銘記", kind = "連結", edge_type = "銘記",
        refs = {"人#柏木秋穗", "事#圖書館"}, value = "她一直記著那件事" }
db:add{ id = "邊#解圍", kind = "連結", edge_type = "當事者",
        refs = {"事#圖書館", "人#佐伯悠"}, value = "出手的人是他" }

ok = pcall(function()
  db:refine("邊#她心動", { segments = {"邊#銘記", "邊#常客"} })   -- 鏈不連續且尾不對
end)
assert(not ok, "③ 段鏈首尾/連續性不符應被擋")

db:refine("邊#她心動", { segments = {"邊#銘記", "邊#解圍"} })
tr = db:trace("邊#她心動")
-- 展開＝粗邊＋兩段、每段再下行到自己的 refs（3＋2×2＝7 節點）
assert(#tr == 7 and tr[2].id == "邊#銘記" and tr[5].id == "邊#解圍",
  "③ trace 沿段展開：粗邊→銘記→(事件)→解圍")
print("③ 連結 LOD：粗邊展開多段、首尾語義守住　OK")

------------------------------------------------------------------ ④ 知識視圖＝overlay：hides 查詢、誤會變體、不可見

local hidden = db:match{ who = "柏木秋穗", state = "hides" }
assert(#hidden == 1 and hidden[1] == "邊#她心動", "④ 「給我她現在可洩的底」")

local truth  = db:resolve("邊#她心動", db:view("柏木秋穗"))
local belief = db:resolve("邊#她心動", db:view("佐伯悠"))
assert(truth == "她對他心動", "④ 她自己看：真相")
assert(belief == "她討厭他（他的誤會）", "④ 他看同一 ID：解析到誤會變體")

local invisible, why = db:resolve("邊#銘記", db:view("佐伯悠"))
assert(invisible == nil and why == "不可見", "④ 他不知道她的銘記邊")
print("④ 知識視圖 overlay：真相/誤會/不可見三態　OK")

------------------------------------------------------------------ ⑤ 執行期寫入門：說不出不知道的事；呈現→canon 鎖

local tx = db:begin()
tx.apply{ speaker = "佐伯悠", act = "評書", refs = {"物#書417", "邊#她心動"} }
local pass, why5 = tx.check()
assert(not pass and why5:find("矛盾防線", 1, true), "⑤ 他引用真相邊應被矛盾防線擋下")
tx.rollback()

tx = db:begin()
tx.apply{ speaker = "佐伯悠", act = "評書", refs = {"物#書417", "邊#誤會"} }
assert(tx.check())
tx.commit()
assert(db.by_id["物#書417"].canon and db.by_id["邊#誤會"].canon, "⑤ 呈現過即 canon")

ok = pcall(function() db:modify("物#書417", "普通文庫") end)
assert(not ok, "⑤ canon 鎖：已呈現不可改")
db:modify("態#書已售", "書#417 已售出（改：被人預訂）")   -- 未呈現可改，玩家無感
print("⑤ 寫入門：矛盾防線＋canon 鎖（呈現才鎖、未呈現可改）　OK")

------------------------------------------------------------------ ⑥ 洩底＝知識轉移：hides→已表達、誤會被真相替換

tx = db:begin()
tx.apply{ speaker = "柏木秋穗", act = "洩底", refs = {"邊#她心動"},
          effects = { reveal = {"邊#她心動"}, to = "佐伯悠" } }
assert(tx.check())
tx.commit()

assert(#db:match{ who = "柏木秋穗", state = "hides" } == 0, "⑥ 洩底後 hides 清空（恰一記帳可據此計數）")
belief = db:resolve("邊#她心動", db:view("佐伯悠"))
assert(belief == "她對他心動", "⑥ 誤會拆除：他的視圖同一 ID 改解析到真相")
assert(db.by_id["邊#她心動"].canon, "⑥ 洩了底＝呈現過＝canon")
print("⑥ 洩底＝知識轉移＋誤會拆除　OK")

------------------------------------------------------------------ ⑦ 排斥邊：兩端不得同真

tx = db:begin()
tx.apply{ speaker = "旁白", act = "描寫", refs = {"態#書在架"} }
assert(tx.check())
tx.commit()

tx = db:begin()
tx.apply{ speaker = "旁白", act = "描寫", refs = {"態#書已售"} }
pass, why5 = tx.check()
assert(not pass and why5:find("排斥邊", 1, true), "⑦ 在架已 canon，已售不得再呈現")
tx.rollback()
print("⑦ 排斥邊：矛盾檢查＝圖結構查詢　OK")

------------------------------------------------------------------ ⑧ speculate：試探分支不落盤；同查詢逐位元相同

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

------------------------------------------------------------------ ⑨ 約束細化：解不出→掛起工單（編譯期 LLM 崗位）

local got, ticket = db:refine("場#校園後方", { grain = "場所級", constraints = { mood = "櫻花" } })
assert(got == nil and ticket:find("掛起工單", 1, true), "⑨ 庫裡沒有滿足點→掛起")
assert(#db.pending == 1)

-- 編譯期補件（LLM 炸候選→人審→入庫的入庫那一步；分佈未鎖，世界為故事讓路）
db:add{ id = "場#櫻花樹下", kind = "節點", parent = "場#校園後方",
        mood = "櫻花", value = "校園後方・櫻花樹下" }
got = db:refine("場#校園後方", { grain = "場所級", constraints = { mood = "櫻花" } })
assert(got == "場#櫻花樹下", "⑨ 補件後同一查詢解出告白級地點")
print("⑨ 約束細化＋掛起工單（告白要在櫻花樹下）　OK")

print("\n九個示範全部通過——事實庫玩具版立住了。")
