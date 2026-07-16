; runtime-modify.scm — s7 執行期修改：程式跑著就重定義，不重啟。
; 跑：source ~/dev/env.sh 後  llm-s7 runtime-modify.scm

; ① 重定義傳播：早定義的 caller，set! 換掉 greet 後立即走新版
(define (greet n) (string-append "v1：嗨 " n))
(define (caller n) (greet n))
(display (string-append "① 改前：" (caller "星野"))) (newline)
(set! greet (lambda (n) (string-append "v2：喲 " n "～（執行期改過）")))  ; 執行期換
(display (string-append "① 改後：" (caller "星野"))) (newline)          ; caller 立即用新版

; ② 套到 cllm：重定義「怎麼問」的包裝，下次呼叫即換行為
(define (ask-wrapped ep) (string-append "[v1] " (llm-ask "自我介紹" ep)))
(set! ask-wrapped (lambda (ep) (string-append "[v2 改過] " (llm-ask "用一句話自我介紹" ep))))

(display "② 互動玩法：llm-s7 不給腳本參數即進「活的 Scheme REPL」——貼新 (define …) 當場重定義；") (newline)
(display "   嵌入端則用 s7_eval_c_string 在執行期灌新碼。") (newline)
(display "   ⚠ s7 無記憶體映像 dump（不像 SBCL）：改動無法「存成映像」，要固化＝重編執行檔（見 baked.c）。") (newline)
