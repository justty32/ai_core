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

(exit 0)
