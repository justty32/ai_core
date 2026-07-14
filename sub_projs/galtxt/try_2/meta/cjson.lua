---@meta cjson
-- cjson 型別 stub —— 給 lua-language-server 看的「只有型別、沒有實作」定義檔。
--
-- 真身是 native/cjson.c，編進 liblua.a、經 vendor/lua/linit.c 塞進 package.preload，
-- host.exe／build/lua.exe 執行期 require("cjson") 拿到的是 C 模組，本檔從不會被
-- dofile／require 真的載入（---@meta 標記已告訴 lua_ls：這檔只用來標註型別，不是可執行程式）。
--
-- 放這裡＋在 .luarc.json 把 meta/ 掛進 workspace.library，是為了解決兩個問題：
--   1. require("cjson") 在 lua_ls 眼中「找不到模組」→ 有這檔案，require 才解析得到。
--   2. 解析到之後，src/llm.lua 用到的 json.encode／json.decode 才有型別提示與補全，
--      而不是解析到了但拿到一個空 table（無成員可提示）。
--
-- API 對應 native/cjson.c 的 cjson_funcs 登記表（encode / decode）。

local cjson = {}

--- 目前 native 版本號（native/cjson.c 的 luaopen_cjson 設定）。
cjson._version = "0.1.2-c"

---將 Lua 值序列化成 JSON 字串（table 依序列/雜湊自動判斷陣列或物件，nil↔null）。
---@param value any
---@return string json
function cjson.encode(value) end

---把 JSON 字串解析成 Lua 值（物件→table、陣列→序列 table、null→nil）。
---@param text string
---@return any value
function cjson.decode(text) end

return cjson
