;;; json.scm — galtxt try_1：s7 物件 ⇄ JSON 字串（抽自 llm.scm，供其 (load) 用）
;;;
;;; 源頭：s7 playground lib/json.scm 的 json->s7 / s7->json，並替 s7->json 補上
;;;   boolean / null（原版沒有，stream:false 會炸），且 let? 分支不 (reverse obj)
;;;   （此 s7 版不准 reverse let；欄位順序對 API 無所謂）。
;;;
;;; 導出：json->s7（字串→s7 inlet/vector 導航）、s7->json（s7→JSON 寫到 port）、
;;;      json-string（s7→JSON 字串）。

;;; ── 字串字面量掃描（★ 這裡曾有硬 bug，別改回去）
;;;
;;; json->s7 的作法是「把 JSON 文字改寫成 Scheme 運算式、再 eval-string」。原版處理字串時是
;;; 「找下一個 `"`；若它前一個字元是 `\` 就再找下一個」——**只認得一個跳脫引號**。真實 LLM 回應
;;; （尤其 reasoning 模型那段思考鏈）動輒好幾個 `\"`，於是 parser 提早跳出字串、把內文當語法解析，
;;; 撞到 `n2.` 之類就吐 `bad entry`，整包回應解不出來（llm-entry 回 #f）。離線 fixture 是我們自己寫的、
;;; 剛好沒有跳脫引號，所以這個 bug **只有打真後端才會現形**。
;;;
;;; 現在改成逐字掃描、正確處理反斜線跳脫，並把 JSON 的跳脫序列**解碼成真正的字元**
;;; （`\"` `\\` `\/` `\n` `\t` `\r` `\b` `\f` `\uXXXX`＋surrogate pair，語意對齊 try_2 的 native cjson），
;;; 再用 s7 的 `write` 印成合法的 Scheme 字串字面量——escape 的正確性交給 s7 自己，不再手工拼接。
;;; （順帶修掉原版另一個潛在雷：`\uXXXX` 直接照抄進 Scheme 字面量，s7 根本讀不懂。）

(define (json-utf8-string cp)          ; Unicode 碼位 → UTF-8 位元組字串（s7 字串是位元組串）
  (cond ((< cp #x80)    (string (integer->char cp)))
        ((< cp #x800)   (string (integer->char (+ #xC0 (quotient cp 64)))
                                (integer->char (+ #x80 (remainder cp 64)))))
        ((< cp #x10000) (string (integer->char (+ #xE0 (quotient cp 4096)))
                                (integer->char (+ #x80 (remainder (quotient cp 64) 64)))
                                (integer->char (+ #x80 (remainder cp 64)))))
        (else           (string (integer->char (+ #xF0 (quotient cp 262144)))
                                (integer->char (+ #x80 (remainder (quotient cp 4096) 64)))
                                (integer->char (+ #x80 (remainder (quotient cp 64) 64)))
                                (integer->char (+ #x80 (remainder cp 64)))))))

;; str 的 i 指著開頭的 `"`。回傳 (解碼後的字串 . 收尾引號的索引)。
(define (json-scan-string str i len)
  (let ((out (open-output-string)))
    (let loop ((k (+ i 1)))
      (cond
       ((>= k len)                     ; 沒收尾引號＝回應被截斷
        (format *stderr* "json: 字串沒有收尾引號（回應可能被截斷）~%")
        (cons (get-output-string out) (- len 1)))
       ((char=? (str k) #\")
        (cons (get-output-string out) k))
       ((and (char=? (str k) #\\) (< (+ k 1) len))
        (let ((e (str (+ k 1))))
          (case e
            ((#\") (write-char #\" out)              (loop (+ k 2)))
            ((#\\) (write-char #\\ out)              (loop (+ k 2)))
            ((#\/) (write-char #\/ out)              (loop (+ k 2)))
            ((#\n) (write-char #\newline out)        (loop (+ k 2)))
            ((#\t) (write-char #\tab out)            (loop (+ k 2)))
            ((#\r) (write-char #\return out)         (loop (+ k 2)))
            ((#\b) (write-char (integer->char 8) out)  (loop (+ k 2)))
            ((#\f) (write-char (integer->char 12) out) (loop (+ k 2)))
            ((#\u)
             (let ((hi (and (<= (+ k 6) len)
                            (string->number (substring str (+ k 2) (+ k 6)) 16))))
               (cond
                ((not hi) (write-char e out) (loop (+ k 2)))   ; 壞的 \u，原樣吐出
                ;; surrogate pair：\uD800-\uDBFF 後面應接 \uDC00-\uDFFF
                ((and (>= hi #xD800) (<= hi #xDBFF) (<= (+ k 12) len)
                      (char=? (str (+ k 6)) #\\) (char=? (str (+ k 7)) #\u))
                 (let ((lo (string->number (substring str (+ k 8) (+ k 12)) 16)))
                   (if (and lo (>= lo #xDC00) (<= lo #xDFFF))
                       (begin
                         (display (json-utf8-string
                                   (+ #x10000 (* 1024 (- hi #xD800)) (- lo #xDC00))) out)
                         (loop (+ k 12)))
                       (begin (display (json-utf8-string hi) out) (loop (+ k 6))))))
                (else (display (json-utf8-string hi) out) (loop (+ k 6))))))
            (else (write-char e out) (loop (+ k 2))))))      ; 未知跳脫：吐出被跳脫的字元
       (else (write-char (str k) out) (loop (+ k 1)))))))

(define json->s7
  (let ()
    (define (strlet . args)            ; inlet with keys as strings
      (apply inlet (do ((p args (cddr p))
                        (fields ()))
                       ((null? p)
                        (reverse! fields))
                     (set! fields (cons (cadr p)
                                        (cons (symbol (car p))
                                              fields))))))
    (let ((jlet (curlet)))
      (lambda (str)
        (do ((p (open-output-string))
             (len (length str))
             (i 0 (+ i 1)))
            ((= i len)
             (let ((result (eval-string (get-output-string p) jlet)))
               (close-output-port p)
               result))
          (case (str i)
            ((#\{) (display "(strlet " p))
            ((#\[) (display "(vector " p))
            ((#\} #\]) (write-char #\) p))
            ((#\: #\,) (write-char #\space p))
            ((#\")
             (let* ((r    (json-scan-string str i len))
                    (endq (cdr r)))
               (write (car r) p)         ; ★ 交給 s7 的 write 產生合法 Scheme 字串字面量
               (set! i endq)))
            ((#\t)
             (if (and (< i (- len 3)) (string=? (substring str i (+ i 4)) "true"))
                 (begin (display "#t" p) (set! i (+ i 3)))
                 (format *stderr* "bad entry: ~S~%" (substring str i))))
            ((#\n)
             (if (and (< i (- len 3)) (string=? (substring str i (+ i 4)) "null"))
                 (begin (display "()" p) (set! i (+ i 3)))
                 (format *stderr* "bad entry: ~S~%" (substring str i))))
            ((#\f)
             (if (and (< i (- len 4)) (string=? (substring str i (+ i 5)) "false"))
                 (begin (display "#f" p) (set! i (+ i 4)))
                 (format *stderr* "bad entry: ~S~%" (substring str i))))
            (else (write-char (str i) p))))))))

(define* (s7->json obj (port (current-output-port)))
  (case (type-of obj)
    ((integer? float?) (display obj port))
    ((boolean?)        (display (if obj "true" "false") port))   ; ← 補：JSON boolean
    ((null?)           (display "null" port))                    ; ← 補：JSON null
    ((string?)         (write obj port))
    ((vector? float-vector? int-vector? byte-vector?)
     (let ((len (length obj)))
       (if (zero? len)
           (display "[]" port)
           (begin
             (write-char #\[ port)
             (do ((i 0 (+ i 1)))
                 ((= i (- len 1)) (s7->json (obj i) port) (write-char #\] port))
               (s7->json (obj i) port)
               (display ", " port))))))
    ((let?)
     (let ((len (length obj)))
       (if (zero? len)
           (display "{}" port)
           (let ((slot-ctr 1))
             (write-char #\{ port)
             (for-each (lambda (slot)
                         (write (symbol->string (car slot)) port)
                         (display " : " port)
                         (s7->json (cdr slot) port)
                         (if (< slot-ctr len) (display ", " port) (write-char #\} port))
                         (set! slot-ctr (+ slot-ctr 1)))
                       obj)))))          ; 不 reverse（此 s7 版不准 reverse let；欄位順序對 API 無所謂）
    (else (format *stderr* "s7->json: bad entry: ~S~%" obj))))

(define (json-string obj)              ; s7 物件 → JSON 字串
  (let ((p (open-output-string)))
    (s7->json obj p)
    (get-output-string p)))
