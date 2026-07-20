-- test/basic.lua —— 離線測（不觸網、不需 binding）：驗失敗分流邏輯。
-- 跑：lua test/basic.lua      （從專案根目錄）
package.path = "./?.lua;./?/init.lua;" .. package.path
local app = require("src.app")

local fails = 0
local function check(cond, name)
  if cond then
    print("  ok  " .. name)
  else
    fails = fails + 1
    print("  FAIL " .. name)
  end
end

local function kind_of(m) return (app.classify_error(m)) end

check(kind_of("後端錯誤 (HTTP 401): Unauthorized") == "auth", "401 → auth")
check(kind_of("authentication_error: invalid x-api-key") == "auth", "authentication → auth")
check(kind_of("缺 Authorization") == "auth", "authorization → auth")
check(kind_of("curl: (7) Failed to connect to 127.0.0.1 port 8787: Connection refused") == "sidecar",
  "refused → sidecar")
check(kind_of("model produced garbage") == "other", "unknown → other")
check(kind_of(nil) == "other", "nil → other 不爆")

-- binding 未就緒時 ask 要走 no-binding、不觸網
if not app.binding_ready() then
  local r = app.ask("hi", { endpoint = "http://127.0.0.1:8787/v1/chat/completions" })
  check(r.kind == "no-binding", "binding 未就緒 → no-binding")
else
  print("  --  binding 已就緒，跳過 no-binding 測（此環境已 source env.sh）")
end

if fails == 0 then
  print("basic.lua：全綠")
  os.exit(0)
else
  print(("basic.lua：%d 項失敗"):format(fails))
  os.exit(1)
end
