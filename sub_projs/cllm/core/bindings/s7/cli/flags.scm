; flags.scm — 反射旗標表與 --help 用法文字。
;   對齊 core-py 的 flags.py（＝C++ cli_flags.cpp）：連線／取樣旗標逐項對應 Client 欄位。
; 反射旗標＝連線／取樣選項；固定旗標（--stream/--image/…）的解析在 argv.scm。

; 反射旗標表：每列 (旗標 名稱 型別)。
;   名稱＝binding keyword 用（帶 '-'，如 api-key → :api-key）；config 檔鍵＝底線版（api_key）。
;   型別∈ {str int float}。順序／欄位對齊 llm::abi::Client（cabi.hpp）。
(define REFLECT-FLAGS
  (list (list "--endpoint"          "endpoint"          'str)
        (list "--api-key"           "api-key"           'str)
        (list "--timeout-ms"        "timeout-ms"        'int)
        (list "--model"             "model"             'str)
        (list "--temperature"       "temperature"       'float)
        (list "--top-p"             "top-p"             'float)
        (list "--presence-penalty"  "presence-penalty"  'float)
        (list "--frequency-penalty" "frequency-penalty" 'float)
        (list "--max-tokens"        "max-tokens"        'int)
        (list "--seed"              "seed"              'int)))

; 帶 '-' 名稱 → binding keyword（:endpoint / :api-key …）。
(define (flag-keyword name) (symbol->keyword (string->symbol name)))
; 帶 '-' 名稱 → config 檔／底線鍵（api-key → api_key）。
(define (flag-config-key name)
  (list->string (map (lambda (c) (if (char=? c #\-) #\_ c)) (string->list name))))

; --help 用的欄位標註（對齊 cli_flags.cpp 的 field_annot；範圍為 OpenAI 慣例值）。
(define FIELD-ANNOT
  (list (cons "temperature"       "，範圍 0.0–2.0，例 0.7（越大越發散）")
        (cons "top-p"             "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）")
        (cons "presence-penalty"  "，範圍 -2.0–2.0，例 0.0")
        (cons "frequency-penalty" "，範圍 -2.0–2.0，例 0.0")
        (cons "max-tokens"        "，≥1，例 512（⚠ reasoning 模型建議不設）")
        (cons "seed"              "，例 42（固定可重現）")
        (cons "timeout-ms"        "，≥0（0＝不設限），例 120000")
        (cons "endpoint"          "，例 http://localhost:1234/v1/chat/completions")
        (cons "model"             "，例 local-model（雲端填真實 model id）")
        (cons "api-key"           "，雲端 API 必給")))
(define (annot-of name) (let ((p (assoc name FIELD-ANNOT))) (if p (cdr p) "")))
(define (type-hint t) (case t ((str) "<字串>") ((int) "<整數>") ((float) "<小數>") (else "")))

; 右補空白到寬 n（s7 format 無 ~nA 寬度修飾，手動對齊旗標欄）。
(define (pad-right s n)
  (let ((len (string-length s)))
    (if (>= len n) s (string-append s (make-string (- n len) #\space)))))

; 印 --help（對齊 cli_flags.cpp 的 print_usage；輸出到 stderr／current-error-port）。
(define (print-usage)
  (let ((e (current-error-port)))
    (display
"用法：llm-s7 cli/main.scm [旗標...] [prompt...]   # 旗標與 prompt 可交錯
  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有
  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。
  範例：llm-s7 cli/main.scm 用一句話介紹你自己
        cat report.txt | llm-s7 cli/main.scm 總結這份
        git diff | llm-s7 cli/main.scm 把 - 寫成 commit 訊息

固定旗標：
  --system <文字>        system role 指示（字面文字；長文用 --system \"$(cat persona.txt)\"）
  --stream               串流逐段吐 stdout（布林，無值）
  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:/http(s):// URL 直通；
                         .json 檔＝預先算好的 media 描述子（{\"url\":…} 或 {\"mime\":…,\"data\":…}）；
                         其餘＝二進位圖檔路徑，讀檔＋mime（由副檔名推），交內核 base64
  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出（收字面非路徑；長內容用 $(cat s.json)）
  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）
  --tool <JSON>          字面工具定義 JSON（可重複）：{\"name\",\"description\",\"parameters\"}
  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或 audio={\"voice\":\"alloy\"}
  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄
  --                     分隔符：其後所有參數一律當 prompt
  --help, -h             顯示本說明

連線／取樣旗標（對應 Client 欄位，未給即不送、交後端默認）：
" e)
    (for-each
     (lambda (row)
       (display (string-append "  " (pad-right (car row) 22) " "
                               (type-hint (caddr row)) (annot-of (cadr row)) "\n") e))
     REFLECT-FLAGS)
    (format e "~%（數值範圍為 OpenAI 慣例，實際上下限依後端而定。）~%")
    (format e "設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。~%")
    (format e "config 檔路徑：--config ＞ 環境變數 ~A ＞ ~~/.config/llm/config.json。~%" CONFIG-ENV-VAR)
    (format e "離線自測：--endpoint file://<絕對路徑> 指向 fixtures 的假回應。~%")))
