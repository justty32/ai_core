--[[ output.lua — llm.ask 的四個出口回呼打包成 Sink（對齊 core/src/cli_output.cpp 的 Sink）。

把「輸出接線 + 其共享狀態」收成一個物件：文字吐 stdout、tool_calls 一行一則 JSON、產出媒體落檔
--media-out、錯誤吐 stderr。回呼一律回 false（不主動中止）。收尾狀態（wrote_text/had_error/
media_err）由 cli.lua 讀來定退出碼。 ]]
local json = require("dkjson")
local media = require("media")

local M = {}

--- 建 Sink：回一個 table，其 on_delta/on_tool/on_media/on_error 為可直接傳給 llm.ask 的閉包。
function M.new(media_out_dir)
  local self = {
    media_out_dir = media_out_dir,
    wrote_text = false,  -- 有無吐過文字（決定要不要補尾端換行）
    had_error = false,   -- on_error 被呼叫過＝請求真失敗
    media_err = false,   -- 媒體落檔失敗＝結果真掉了
    media_n = 0,         -- 已落檔媒體數（供編號檔名）
  }

  self.on_delta = function(piece)
    io.stdout:write(piece); io.stdout:flush()
    self.wrote_text = true
    return false
  end

  -- tool_calls 一行一則 JSON：{"tool","id","arguments"}（arguments 原樣內嵌解出的物件）。
  self.on_tool = function(call)
    local args_obj = json.decode(call.arguments or "{}")
    if args_obj == nil then args_obj = call.arguments end  -- 後端理應保證 JSON；壞了原樣塞字串
    local line = json.encode({ tool = call.name, id = call.id, arguments = args_obj },
      { keyorder = { "tool", "id", "arguments" } })
    io.stdout:write(line .. "\n"); io.stdout:flush()
    return false
  end

  self.on_media = function(m)
    if not self.media_out_dir then  -- 沒地方放＝明說丟棄
      io.stderr:write(string.format("收到產出媒體（%s，%d bytes）但沒給 --media-out，已丟棄\n",
        m.mime, #m.bytes))
      return false
    end
    self.media_n = self.media_n + 1
    local path = string.format("%s/llm-media-%d.%s", self.media_out_dir, self.media_n, media.ext_of(m.mime))
    local f = io.open(path, "wb")
    if not f then
      io.stderr:write("媒體落檔失敗：" .. path .. "\n"); self.media_err = true; return false
    end
    f:write(m.bytes); f:close()
    io.stdout:write(path .. "\n"); io.stdout:flush()
    return false
  end

  self.on_error = function(msg)
    self.had_error = true
    io.stderr:write("請求失敗：" .. msg .. "\n")
  end

  return self
end

return M
