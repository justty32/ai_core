---@meta http
-- http 型別 stub —— 給 lua-language-server 看的「只有型別、沒有實作」定義檔。
--
-- 真身是 native/http.c（Windows=WinHTTP／Linux=libcurl，file:// 兩平台皆自理），編進
-- liblua.a、經 vendor/lua/linit.c 塞進 package.preload；本檔從不會被真的載入，只供
-- lua_ls 解析 require("http") 與標註 http.request／http.stream 的型別（見 meta/cjson.lua
-- 開頭同一段說明，兩檔道理一致）。
--
-- API 對應 native/http.c 檔頭註解與 http_funcs 登記表（request / stream）。

---@class http.Options
---@field url string 完整 URL（含 file:// 特例，兩平台皆支援離線 fixture 讀取）
---@field method? string HTTP 方法，預設 GET/POST 依 body 是否存在由 C 端決定
---@field headers? string[] 逐行 "K: V" 字串陣列
---@field body? string 請求本體（通常是 cjson.encode 過的 JSON 字串）
---@field timeout? integer 逾時毫秒
---@field on_data? fun(chunk: string) 串流專用：收到的 raw bytes 逐塊回呼（http.stream 才用得到）

local http = {}

--- 目前 native 版本號（native/http.c 的 luaopen_http 設定）。
http._version = "0.1"

---非串流請求：整包收完才回傳。傳輸失敗會 raise（呼叫端自行 pcall）。
---@param opts http.Options
---@return integer status
---@return string body
function http.request(opts) end

---串流請求：內容經 opts.on_data 逐塊即時餵回，回傳值只有 status（完成時）。
---@param opts http.Options
---@return integer status
function http.stream(opts) end

return http
