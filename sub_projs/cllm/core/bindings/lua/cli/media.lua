--[[ media.lua — --image/--media 的 MIME 對照與三分流取值。

mime_of／ext_of 對齊 cli_config.cpp 的同名對照表。build_media_item 是取值分流：讓 --media 除了讀
二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次重算 base64
（對齊 core-py media.py）。回的 item table 直接餵 llm.ask 的 media 欄（欄位 url/mime/bytes）。 ]]
local json = require("dkjson")
local internal = require("internal")

local M = {}

--- 副檔名 → MIME（對齊 cli_config.cpp 的 mime_of）。
function M.mime_of(path)
  local ext = (path:match("%.([^.]+)$") or ""):lower()
  return ({ png = "image/png", jpg = "image/jpeg", jpeg = "image/jpeg",
            gif = "image/gif", webp = "image/webp", wav = "audio/wav",
            mp3 = "audio/mpeg" })[ext] or "application/octet-stream"
end

--- MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of）。
function M.ext_of(mime)
  return ({ ["image/png"] = "png", ["image/jpeg"] = "jpg", ["image/gif"] = "gif",
            ["image/webp"] = "webp", ["audio/wav"] = "wav", ["audio/mpeg"] = "mp3" })[mime] or "bin"
end

--- --image/--media 一個值 → media item（三分流）。成功回 table、失敗印 stderr 回 nil。
--- 1. data:／http(s):// URL → 直接當 url 送、不編碼。
--- 2. .json 副檔名 → 讀該檔解 JSON，當「預先算好的 media 描述子」直通：{url=…} 或 {mime=…,data=<base64>}。
--- 3. 其餘（二進位圖檔路徑）→ 讀檔＋交內核 base64（mime 由副檔名推）。
function M.build_media_item(spec)
  local low = spec:lower()
  if spec:sub(1, 5) == "data:" or low:sub(1, 7) == "http://" or low:sub(1, 8) == "https://" then
    return { url = spec }  -- URL 直通
  end
  if low:sub(-5) == ".json" then  -- 預先算好的 media 描述子
    local body = internal.read_file_text(spec)
    if not body then
      io.stderr:write(string.format("讀不到檔案：%s（--image/--media 描述子）\n", spec)); return nil
    end
    local desc = json.decode(body)
    if desc == nil then
      io.stderr:write(string.format("media 描述子 JSON 解析失敗（%s）\n", spec)); return nil
    end
    if type(desc) == "table" and desc.url then
      return { url = desc.url }  -- 直通 url、不編碼
    end
    if type(desc) == "table" and desc.mime and desc.data ~= nil then
      return { url = "", mime = desc.mime, bytes = internal.b64decode(desc.data) }
    end
    io.stderr:write(string.format(
      "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（%s）\n", spec)); return nil
  end
  local raw = internal.read_file_bytes(spec)  -- 二進位圖檔：讀檔 + 內核 base64
  if not raw then
    io.stderr:write(string.format("讀不到檔案：%s（--image/--media）\n", spec)); return nil
  end
  return { url = "", mime = M.mime_of(spec), bytes = raw }
end

return M
