-- src/app.lua —— 核心模組（純邏輯，別人 require 這個）。
--
-- 只做兩件事：
--   1. 動態解析 cllm 的 Lua binding（require("llm") 提供的 llm.ask{...}），
--      即使 binding 尚未就緒也不會載入期爆掉——讓 --help / --check 能離線跑。
--   2. 呼 llm.ask，並把失敗照 tools/INTEGRATION.md 分流：
--         401 / authentication  → 憑證問題（沒登入 / key 失效）→ 要登入或填 key
--         connection refused    → sidecar 沒起（proxy 沒跑）
--      這兩類必須分清楚，別當成「模型沒回應」。

local M = {}

-- ── binding 解析 ──────────────────────────────────────────────
-- cllm 的 Lua binding 是 llm.so，掛在 LUA_CPATH_5_4／LUA_CPATH_5_5（見 ~/dev/cllm/env.sh）。
-- 用 pcall(require, ...) 動態載，避免「模組還沒好／環境沒 source」時整支載不進來。
-- llm.ask{...}：table 形式，成功回文字字串；後端錯誤回 nil, errmsg（errmsg 已含「401」等可分流字串）。
function M.resolve_ask()
  local ok, llm = pcall(require, "llm")
  if not ok or type(llm) ~= "table" or type(llm.ask) ~= "function" then
    return nil
  end
  return llm.ask
end

function M.binding_ready()
  return M.resolve_ask() ~= nil
end

-- ── 失敗分流 ──────────────────────────────────────────────────
-- 把 llm.ask 的錯誤訊息分成三類："auth" / "sidecar" / "other"。回 kind, 人話說明。
function M.classify_error(msg)
  local raw = msg or ""
  local m = raw:lower()
  local function has(s) return m:find(s, 1, true) ~= nil end
  if has("401") or has("authentication") or has("unauthorized") or has("authorization")
     or raw:find("未登入", 1, true) or raw:find("token 失效", 1, true) then
    return "auth",
      "憑證問題（HTTP 401 / 未授權）：沒登入或 api_key 失效。\n" ..
      "    → Anthropic 直連：確認 config 的 api_key 是有效的 sk-ant-... 金鑰。\n" ..
      "    → 若走 OpenRouter OAuth：跑 `llm-login login` 重新帳號登入。"
  elseif has("connection refused") or has("couldn't connect") or has("could not connect")
     or has("connection reset") or has("failed to connect") then
    return "sidecar",
      "連線被拒（connection refused）：anthropic-proxy sidecar 沒在跑。\n" ..
      "    → 先起 proxy：scripts/up.sh 會自動起。"
  else
    return "other", "其他錯誤：" .. (raw ~= "" and raw or "unknown")
  end
end

-- ── 對外主呼叫 ────────────────────────────────────────────────
-- 問一次 LLM。opts 是 table：
--   endpoint  cllm endpoint（proxy 或廠商官方）
--   api_key   Bearer token（sk-ant-... 或 OAuth 換來的 key）
--   model     模型 id（如 claude-opus-4-8）
--   stream    是否串流（真值時逐段進 on_delta）
--   on_delta  function(piece)  串流片段（回 true 可中止）
-- 回傳 table：{ok=true, text=...} 或 {ok=false, kind="auth"/"sidecar"/"other"/"no-binding", error=說明}。
-- classify_error 完全不觸網、不需 binding 就能被單元測試；真呼叫才需要 binding。
function M.ask(prompt, opts)
  opts = opts or {}
  local ask_fn = M.resolve_ask()
  if not ask_fn then
    return {
      ok = false, kind = "no-binding",
      error = "cllm 的 Lua binding 尚未就緒：require(\"llm\") 找不到模組。\n" ..
              "    → source ~/dev/cllm/env.sh 掛上 LUA_CPATH_5_4／5_5 後再跑（見 README 前置）。",
    }
  end

  local captured
  local call = {
    prompt   = prompt,
    endpoint = opts.endpoint,
    api_key  = opts.api_key,
    model    = opts.model,
    stream   = opts.stream and true or false,
    on_delta = opts.on_delta,
    -- 後端錯誤字串一定會被 binding 存進第 2 回傳值；on_error 只是保險再抓一份。
    on_error = function(m) captured = m end,
  }

  -- ask 成功回 1 值（text）；後端錯誤回 nil, errmsg；Lua 回呼拋錯才會 raise → 用 pcall 包。
  local ok, res, err = pcall(ask_fn, call)
  if not ok then
    -- Lua 層例外（例如 on_delta 回呼自己丟錯）
    local kind, explain = M.classify_error(tostring(res))
    return { ok = false, kind = kind, error = explain }
  end
  if res == nil then
    local msg = err or captured or "llm_ask failed"
    local kind, explain = M.classify_error(msg)
    return { ok = false, kind = kind, error = explain }
  end
  return { ok = true, text = res }
end

return M
