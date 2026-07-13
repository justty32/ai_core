;; test.scm — 全離線：curl file:// 灌假回應，驗 request 組裝 + 回應解析（cwd 需在 try_1）
(load "llm.scm")
(set! *llm-base-url* "file:///C:/code/mine/ai_core/sub_projs/galtxt/try_1/fake")
(format #t "=== 呼叫 llm-entry ===~%")
(let ((r (llm-entry :prompt "你是一隻貓娘，請回答我問題"
		    :sys "你是傲嬌貓娘"
		    :temp 0.8 :top-p 0.9 :max-tokens 128)))
  (format #t "content => ~A~%" r))
(format #t "=== 送出的請求 body（回讀暫存檔）===~%~A~%" (slurp "galtxt_llm_req.json"))
(exit 0)
