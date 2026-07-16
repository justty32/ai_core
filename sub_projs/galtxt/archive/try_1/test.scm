;; test.scm — 全離線：curl file:// 灌假回應，驗 request 組裝 + 回應解析（cwd 需在 try_1）
(load "llm.scm")
;; s7 沒有跨平台 getcwd，借道 system 抓目前工作目錄，拼出本機 file:// 絕對路徑
;; （原本寫死 Windows 路徑 C:/code/mine/...，Linux 上不存在、curl 會直接 reject）。
(define *here* (let ((s (system "pwd" #t)))
                (substring s 0 (- (length s) 1))))   ; 去掉 pwd 輸出尾端換行
(set! *llm-base-url* (string-append "file://" *here* "/fake"))
(format #t "=== 呼叫 llm-entry ===~%")
(let ((r (llm-entry :prompt "你是一隻貓娘，請回答我問題"
          :sys "你是傲嬌貓娘"
          :temp 0.8 :top-p 0.9 :max-tokens 128)))
  (format #t "content => ~A~%" r))
(format #t "=== 送出的請求 body（回讀暫存檔）===~%~A~%" (slurp "galtxt_llm_req.json"))

;; ★ JSON 跳脫序列（json.scm 的字串掃描器）——這一關**離線 fixture 原本測不到**：
;;   假回應是我們自己寫的、剛好沒有跳脫引號，所以「只認一個 \" 」的舊 bug 躲過了整套離線測試，
;;   一打真後端（reasoning 模型的思考鏈裡一堆 \"）就整包解不出來。補上這組定樁。
(format #t "~%=== JSON 跳脫序列 ===~%")
(define (check! name got want)
  (format #t "~A ~A~%" (if (string=? got want) "✓" "✗ 失敗！") name)
  (unless (string=? got want)
    (format #t "   得到 ~S~%   應為 ~S~%" got want)))

(let ((j (json->s7 "{\"a\": \"他說 \\\"你好\\\"，再說 \\\"再見\\\"\"}")))
  (check! "多個跳脫引號" (j 'a) "他說 \"你好\"，再說 \"再見\""))
(let ((j (json->s7 "{\"a\": \"第一行\\n第二行\\t欄\"}")))
  (check! "\\n / \\t" (j 'a) (string-append "第一行" (string #\newline) "第二行" (string #\tab) "欄")))
(let ((j (json->s7 "{\"a\": \"反斜線 \\\\ 與斜線 \\/\"}")))
  (check! "\\\\ / \\/" (j 'a) "反斜線 \\ 與斜線 /"))
(let ((j (json->s7 "{\"a\": \"\\u4f60\\u597d\"}")))
  (check! "\\uXXXX（中文）" (j 'a) "你好"))
(let ((j (json->s7 "{\"a\": \"\\ud83d\\ude38\"}")))               ; U+1F638 貓臉
  (check! "surrogate pair（emoji）" (j 'a) "😸"))
(let ((j (json->s7 "{\"a\": \"x\", \"b\": {\"c\": [1, 2, true, false, null]}}")))
  (check! "巢狀／陣列／true/false/null 仍正常"
          (format #f "~A ~A ~A" (j 'a) (((j 'b) 'c) 0) (((j 'b) 'c) 2))
          "x 1 #t"))
(exit 0)
