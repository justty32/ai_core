-- demo_lua.lua — 展示「LLM 直接吐 Lua table，一個 load 就變原生 table」（離線假回應）
-- 跑：host.exe demo_lua.lua
-- 這是 try_2 的關鍵洞見：runtime 就是 Lua，何必讓模型吐 JSON 再拿 parser 解？
-- 讓模型吐 Lua table 字面量 → sandbox load → 回來就是能直接導航／運算的原生 table。
local llm = dofile("llm.lua")
llm.base_url = "file:///C:/code/mine/ai_core/sub_projs/galtxt/try_2/fake_lua"

local ch = llm.llm_entry{
  sys = "你只輸出一個 Lua table，欄位 name(字串) affection(整數) lines(字串陣列)，不要多餘文字。",
  prompt = "生成一個傲嬌 galgame 角色。",
  lua = true,                      -- ★ 開 Lua 格式輸出
}
if not ch then io.stderr:write("解析失敗\n"); os.exit(1) end

print("type(ch)   =", type(ch))            -- table（不是字串！）
print("ch.name    =", ch.name)
print("affection+1 =", ch.affection + 1)   -- 原生 number，能直接運算
for i, line in ipairs(ch.lines) do
  print(("台詞[%d]   = %s"):format(i, line))
end
print("---- llm.dump 回乾淨 Lua 字面量（同像性 round-trip）----")
print(llm.dump(ch))
