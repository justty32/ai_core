; main.scm — s7 CLI 進入點（對齊 core-py cli.py 的 _entry／__main__）。
;   跑：llm-s7 cli/main.scm [旗標...] [prompt...]
;   靠 (port-filename) 自定位本檔所在目錄，依相依序載入姊妹 .scm，再以 *argv* 呼叫 main、exit 帶碼。
;   *argv*＝host（host.c）把 argv[2..] 綁成的字串 list（無程式名元素）。
;   退出碼：0 成功；1 用法錯；2 請求失敗；130 SIGINT（由 host／s7 外層負責，這裡不特別攔）。

; 本檔所在目錄（含結尾 '/'）。載入姊妹檔前先算好（之後 port-filename 會隨載入變動）。
(define *cli-dir*
  (let* ((f (port-filename))
         (slash (let loop ((i (- (string-length f) 1)))
                  (cond ((< i 0) #f)
                        ((char=? (string-ref f i) #\/) i)
                        (else (loop (- i 1)))))))
    (if slash (substring f 0 (+ slash 1)) "./")))

(for-each (lambda (name) (load (string-append *cli-dir* name)))
          '("internal.scm" "flags.scm" "config.scm" "media.scm"
            "output.scm" "argv.scm" "reqinput.scm" "cli.scm"))

(exit (main (if (and (defined? '*argv*) (list? *argv*)) *argv* '())))
