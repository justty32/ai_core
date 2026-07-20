;;;; flags.lisp — 反射旗標表與 --help 用法文字
;;;; （對齊 core-py/cllm/flags.py，其對齊 core/src/cli_flags.cpp）。
;;;;
;;;; 反射旗標＝連線／取樣選項，逐項對應 cllm:ask 的關鍵字參數（同 llm::abi::Client 欄位）。
;;;; 固定旗標（--stream/--image/…）的解析在 argv.lisp。print-usage 印的清單即由本表生成。
(in-package :cllm-cli)

;; 反射旗標表：每列 (旗標字串  config鍵  ask關鍵字  型別  --help 註解)。
;; 順序／欄位對齊 core-py 的 REFLECT_FLAGS（即 llm::abi::Client）。config鍵＝config 檔用的
;; snake_case 名；ask關鍵字＝傳給 cllm:ask 的 keyword；型別供命令列字串轉型與用法提示。
(defparameter *reflect-flags*
  '(("--endpoint"          "endpoint"          :endpoint          :string "，例 http://localhost:1234/v1/chat/completions")
    ("--api-key"           "api_key"           :api-key           :string "，雲端 API 必給")
    ("--timeout-ms"        "timeout_ms"        :timeout-ms        :int    "，≥0（0＝不設限），例 120000")
    ("--model"             "model"             :model             :string "，例 local-model（雲端填真實 model id）")
    ("--temperature"       "temperature"       :temperature       :float  "，範圍 0.0–2.0，例 0.7（越大越發散）")
    ("--top-p"             "top_p"             :top-p             :float  "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）")
    ("--presence-penalty"  "presence_penalty"  :presence-penalty  :float  "，範圍 -2.0–2.0，例 0.0")
    ("--frequency-penalty" "frequency_penalty" :frequency-penalty :float  "，範圍 -2.0–2.0，例 0.0")
    ("--max-tokens"        "max_tokens"        :max-tokens        :int    "，≥1，例 512（⚠ reasoning 模型建議不設）")
    ("--seed"              "seed"              :seed              :int    "，例 42（固定可重現）")))

(defparameter *type-hint*
  '((:string . "<字串>") (:int . "<整數>") (:float . "<小數>")))

(defun reflect-flag (flag)
  "查旗標字串對應的反射列（找不到回 NIL）。"
  (assoc flag *reflect-flags* :test #'string=))

(defun print-usage (&optional (out *error-output*))
  "印 --help（對齊 flags.py 的 print_usage；輸出到 stderr）。"
  (write-string
   "用法：llm [旗標...] [prompt...]        # 旗標與 prompt 可交錯
  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有
  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。
  範例：llm 用一句話介紹你自己
        cat report.txt | llm 總結這份       # prompt＋stdin 合體
        git diff | llm 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點

固定旗標：
  --system <文字>        system role 指示（字面文字；長文用 --system \"$(cat persona.txt)\"）
  --stream               串流逐段吐 stdout（布林，無值）
  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:/http(s):// URL 直接當 url 送；
                         .json 檔＝預先算好的 media 描述子（{\"url\":…} 或 {\"mime\":…,\"data\":…}）直通；
                         其餘＝二進位圖檔路徑，讀檔＋base64（mime 由副檔名推）
  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出（收字面非路徑；長內容用 $(cat s.json)）
  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）
  --tool <JSON>          字面工具定義 JSON（可重複）：{\"name\",\"description\",\"parameters\"}；
                         tool_calls 一行一則 JSON 吐 stdout
  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或 audio={\"voice\":\"alloy\"}（=後收字面 JSON）
  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄
  --                     分隔符：其後所有參數一律當 prompt
  --help, -h             顯示本說明

連線／取樣旗標（對應 Client 欄位，未給即不送、交後端默認）：
" out)
  (dolist (row *reflect-flags*)
    (destructuring-bind (flag cfgkey kw type annot) row
      (declare (ignore cfgkey kw))
      (format out "  ~22a ~a~a~%" flag (cdr (assoc type *type-hint*)) annot)))
  (format out "
設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。
config 檔路徑：--config <檔> ＞ 環境變數 ~a ＞ ~~/.config/llm/config.json。
離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。
" +config-env-var+))
