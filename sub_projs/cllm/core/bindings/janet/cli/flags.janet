# flags.janet — 反射旗標表與 --help 用法文字（對齊 core/src/cli_flags.cpp）。
#
# 反射旗標＝連線／取樣選項，逐項對應 llm::abi::Client 欄位；固定旗標（--stream/--image/…）
# 的解析在 argv.janet。print-usage 印的清單即由本表生成。

(import ./internal :as internal)

# 反射旗標表：[--flag  config鍵(底線,對齊 Client 欄位)  ask關鍵字(連字號,binding 用)  型別]。
# 順序／欄位對齊 llm::abi::Client（cabi.hpp）。config 檔用底線鍵、binding 的 llm/ask 用連字號關鍵字。
(def reflect-flags
  [["--endpoint"          "endpoint"          :endpoint          :string]
   ["--api-key"           "api_key"           :api-key           :string]
   ["--timeout-ms"        "timeout_ms"        :timeout-ms        :int]
   ["--model"             "model"             :model             :string]
   ["--temperature"       "temperature"       :temperature       :float]
   ["--top-p"             "top_p"             :top-p             :float]
   ["--presence-penalty"  "presence_penalty"  :presence-penalty  :float]
   ["--frequency-penalty" "frequency_penalty" :frequency-penalty :float]
   ["--max-tokens"        "max_tokens"        :max-tokens        :int]
   ["--seed"              "seed"              :seed              :int]])

# --flag → [config鍵 ask關鍵字 型別]（argv 掃描用）
(def flag->spec
  (let [t @{}]
    (each [flag ck kw typ] reflect-flags (put t flag [ck kw typ]))
    (table/to-struct t)))

# config 檔鍵（底線）→ ask 關鍵字（config.janet 寫入 client 用）
(def config-key->kw
  (let [t @{}]
    (each [flag ck kw typ] reflect-flags (put t ck kw))
    (table/to-struct t)))

# --help 用的欄位標註（對齊 cli_flags.cpp 的 field_annot；範圍為 OpenAI 慣例值）。
(def field-annot
  {"endpoint"          "，例 http://localhost:1234/v1/chat/completions"
   "api_key"           "，雲端 API 必給"
   "timeout_ms"        "，≥0（0＝不設限），例 120000"
   "model"             "，例 local-model（雲端填真實 model id）"
   "temperature"       "，範圍 0.0–2.0，例 0.7（越大越發散）"
   "top_p"             "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）"
   "presence_penalty"  "，範圍 -2.0–2.0，例 0.0"
   "frequency_penalty" "，範圍 -2.0–2.0，例 0.0"
   "max_tokens"        "，≥1，例 512（⚠ reasoning 模型建議不設）"
   "seed"              "，例 42（固定可重現）"})
(def type-hint {:string "<字串>" :int "<整數>" :float "<小數>"})

(defn print-usage []
  ``印 --help（對齊 cli_flags.cpp 的 print_usage；輸出到 stderr）。``
  (eprin
    ``用法：llm [旗標...] [prompt...]        # 旗標與 prompt 可交錯
  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有
  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。
  範例：llm 用一句話介紹你自己
        cat report.txt | llm 總結這份       # prompt＋stdin 合體
        git diff | llm 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點
        llm --stream -- --開頭的-prompt      # -- 之後一律當 prompt（unix 分隔符）

固定旗標：
  --system <文字>        system role 指示（字面文字；長文用 --system "$(cat persona.txt)"）
  --stream               串流逐段吐 stdout（布林，無值）
  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:/http(s):// URL 直接當 url 送；
                         .json 檔＝預先算好的 media 描述子（{"url":…} 或 {"mime":…,"data":…}）
                         直通、不重算 base64；其餘＝二進位圖檔路徑，讀檔＋base64（mime 由副檔名推）
  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出（⚠ 收字面非路徑；長內容用 $(cat s.json)）
  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）
  --tool <JSON>          字面工具定義 JSON（可重複）：{"name","description","parameters"}；
                         ⚠ 收字面非路徑，長內容用 $(cat t.json)；tool_calls 一行一則 JSON 吐 stdout
  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或 audio={"voice":"alloy"}（=後收字面 JSON）
  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄
  --                     分隔符：其後所有參數一律當 prompt
  --help, -h             顯示本說明

連線／取樣旗標（對應 Client 欄位，未給即不送、交後端默認）：
``)
  (each [flag ck kw typ] reflect-flags
    (eprintf "  %-22s %s%s" flag (type-hint typ) (get field-annot ck "")))
  (eprin
    (string/format
      ``
（數值範圍為 OpenAI 慣例，實際上下限依後端而定。）

設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。
config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。
  （env 只用來指定 config 檔路徑，不存任何設定值。）
離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。
``
      internal/config-env-var)))
