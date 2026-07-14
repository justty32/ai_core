;;;; comfy.lisp — 舒適層本體
;;;;
;;;; 目標：讓寫 Common Lisp 更順手，但**不魔改**標準語意——只加糖，不減味。
;;;;
;;;;   (1) 布林糖：true → t，false → nil
;;;;       純常數，不需 readtable；import :comfy 的 true/false 即可用。
;;;;
;;;;   (2) 字元字面量：'a' → #\a
;;;;       靠一個蓋在 #\' 上的 reader macro。難處在於 ' 本來就是 quote，
;;;;       所以做法是「先讀一個字元，看它後面是不是收尾的 '」來分辨：
;;;;         'a'      → 後面緊跟 ' → 字元 #\a
;;;;         'a  'foo '(1 2)  → 後面不是 ' → 退回原本的 quote 行為
;;;;       另支援 C 風格轉義： '\n' '\t' '\r' '\a' '\b' '\f' '\v' '\0' '\\' '\'' '\s'(空白)。
;;;;
;;;;   (3) C 風格字串轉義： "a\nb" → a、換行、b
;;;;       標準 CL 字串裡反斜線只 escape " 與 \ 自己，故 "a\nb" 是字面 a n b（不含換行），
;;;;       C 慣用者會踩坑。蓋一個 reader 在 #\" 上改成 C 語意：支援
;;;;       \n \t \r \a \b \f \v \0 \\ \" \' 與 \xHH（兩位十六進位）。
;;;;       數字與其餘資料形式與標準 CL 一致，不動。
;;;;
;;;;   (2)(3) 的 reader 只裝在 *comfy-readtable* 裡，要用才 enable，不污染全域。

(in-package #:comfy)

;;; ── (1) 布林糖 ──────────────────────────────────────────────────────
(defconstant true  t   "順手的布林真——就是 CL 的 t。")
(defconstant false nil "順手的布林假——就是 CL 的 nil。")

;;; ── C 風格轉義（字元字面量與字串共用）───────────────────────────────

(defun %comfy-c-escape (c)
  "把 \\X 的轉義字元 X 映成實際字元；無對應者原樣返回（含 \\ 與 \" 與 '）。"
  (case c
    (#\n #\Newline)
    (#\t #\Tab)
    (#\r #\Return)
    (#\a (code-char 7))    ; bell / alert
    (#\b #\Backspace)
    (#\f #\Page)           ; formfeed
    (#\v (code-char 11))   ; vertical tab
    (#\0 #\Nul)
    (#\s #\Space)          ; comfy 附贈：字元字面量方便寫空白（C 無此，無害）
    (t   c)))              ; \\ → \、\" → "、\' → '、其餘 \X → X

(defun comfy-quote-reader (stream char)
  "蓋在 #\\' 上：分辨『字元字面量 'X'』與『一般 quote』。
   - 'X'      → 字元物件 #\\X（X 可為 \\轉義）
   - 'form    → (quote form)，與標準 quote 完全一致
   讀取器已消耗開頭的 '，我們往下探一格來決定走哪條路。"
  (declare (ignore char))
  (let ((c1 (read-char stream t nil t)))
    (flet ((finish (ch)
             ;; 字元字面量必須以 ' 收尾，否則語法錯
             (let ((close (read-char stream t nil t)))
               (unless (char= close #\')
                 (error "comfy：字元字面量未以 ' 收尾（讀到 ~S）。" close))
               ch)))
      (cond
        ;; 'X\...'：反斜線轉義字元字面量
        ((char= c1 #\\)
         (finish (%comfy-c-escape (read-char stream t nil t))))
        ;; 'X'：後面緊跟收尾 ' → 字元字面量
        ((let ((c2 (peek-char nil stream nil nil t)))
           (and c2 (char= c2 #\')))
         (finish c1))
        ;; 否則：退回標準 quote。把 c1 塞回去，讀完整個 form 包成 (quote …)。
        (t
         (unread-char c1 stream)
         (list 'quote (read stream t nil t)))))))

;;; ── (3) C 風格字串讀取器 ────────────────────────────────────────────

(defun %read-hex (stream n)
  "讀 n 個十六進位字元、回傳整數（供 \\xHH 用）。"
  (let ((s (make-string n)))
    (dotimes (i n) (setf (char s i) (read-char stream t nil t)))
    (parse-integer s :radix 16)))

(defun comfy-string-reader (stream ch)
  "蓋在 #\\\" 上：讀 C 語意字串——反斜線走 C 轉義（\\n \\t … \\xHH），
   不像標準 CL 只認 \\\" 與 \\\\。讀到未轉義的 \" 收尾。"
  (declare (ignore ch))
  (let ((out (make-string-output-stream)))
    (loop
      (let ((c (read-char stream t nil t)))
        (cond
          ((char= c #\") (return (get-output-stream-string out)))
          ((char= c #\\)
           (let ((e (read-char stream t nil t)))
             (if (char-equal e #\x)
                 (write-char (code-char (%read-hex stream 2)) out)   ; \xHH：兩位十六進位
                 (write-char (%comfy-c-escape e) out))))
          (t (write-char c out)))))))

(defvar *comfy-readtable*
  (let ((rt (copy-readtable nil)))
    (set-macro-character #\' #'comfy-quote-reader  nil rt)  ; nil＝終止性巨集字元（同標準 quote）
    (set-macro-character #\" #'comfy-string-reader nil rt)  ; C 風格字串轉義
    rt)
  "裝好 'a' 字元讀取器＋C 風格字串轉義的 readtable；數字與其餘語法與標準 CL 一致。")

(defun enable-comfy-syntax ()
  "把當前 *readtable* 換成 comfy 版。給檔頭在
     (eval-when (:compile-toplevel :load-toplevel :execute) (comfy:enable-comfy-syntax))
   裡呼叫，之後該檔就能用 'a' 字元語法。回傳舊 readtable 方便還原。"
  (prog1 *readtable*
    (setf *readtable* *comfy-readtable*)))

(defmacro with-comfy-syntax (&body body)
  "在 body **讀取期**啟用 comfy 語法。注意 Lisp 是先讀完整段才求值，
   故本巨集主要在 REPL／read-from-string 這類逐段讀取的場景有意義；
   要讓整個原始碼檔用 comfy 語法，請在檔頭用 enable-comfy-syntax（見上）。"
  `(let ((*readtable* *comfy-readtable*))
     ,@body))
