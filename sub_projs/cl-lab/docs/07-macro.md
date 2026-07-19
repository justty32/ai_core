# 07 · 巨集 macro

CL 的巨集是 **Lisp 宏系統的祖宗**：macro 是「在編譯期執行、產生程式碼」的函式。
函式處理**值**，macro 處理**還沒求值的程式碼**。

## 三個符號

| 符號 | 名稱 | 作用 |
|------|------|------|
| `` ` `` | quasiquote（反引號） | 開一個「程式碼模板」，內容預設不求值 |
| `,` | unquote | 在模板裡「挖洞」，把這個求值後填進去 |
| `,@` | unquote-splicing | 同上，但把一個**串列攤平**插入 |

## 最簡單的例子

```lisp
(defmacro my-when (test &body body)
  `(if ,test (progn ,@body)))       ; ,test 填進去、,@body 把多句攤平

(my-when t (foo) (bar))
;; 展開成 → (if t (progn (foo) (bar)))
```

`&body` 跟 `&rest` 幾乎一樣（收集剩餘參數），但慣例用 `&body` 表示「這是一段程式碼主體」，
編輯器縮排也會據此對待。

## 為什麼這非得是 macro？函式辦不到的事

新手最容易卡的一問：「macro 不就是一個會處理別的程式碼的函式嗎？我用函式寫不行？」
用一個例子就講清楚——**試著自己寫一個 `if`**：

```lisp
(defun my-if (test then else)      ; ✗ 行不通
  (if test then else))

(my-if t (print "yes") (print "no"))
;; 印出 yes，也印出 no ——兩個分支都先跑完了才進函式
```

關鍵在**函式的參數會先求值**。進到 `my-if` 之前，`(print "yes")` 和 `(print "no")` 已經各跑一次，
函式拿到的是兩個「已經發生」的結果，救不回來。而 `if` 的整個重點就是**只執行選中的那條分支**——
所以它不可能是函式，只能是特殊形式或 macro。

macro 收到的是**還沒求值的程式碼**（上面的 `,then` `,else` 就是那段原封不動的碼），由你決定誰求值、
何時求值。剛才的 `my-when` 正是如此：body 被包進 `progn` 塞到 `if` 的一條分支裡，沒中選的路根本不會跑。
**需要控制求值時機，就是需要 macro 的訊號。**

## 用 gensym 避免變數捕捉（衛生）

macro 展開若引入區域變數，可能不小心蓋到使用者的同名變數。用 `gensym` 產生獨一無二的符號：

```lisp
(defmacro swap (a b)
  (let ((tmp (gensym)))              ; tmp 綁到一個全世界獨一無二的符號
    `(let ((,tmp ,a))
       (setf ,a ,b
             ,b ,tmp))))

(let ((x 1) (y 2)) (swap x y) (list x y))   ; => (2 1)
```

展開後長這樣（`G111` 是 gensym 生的，不可能撞名）：

```lisp
(let ((G111 a)) (setf a b b G111))
```

## `,@` splice

```lisp
(defmacro run-all (&rest forms)
  `(progn ,@forms))                  ; 把 forms 這個串列攤平進 progn

(run-all (print 1) (print 2))
;; → (progn (print 1) (print 2))
```

## 除錯 macro 的關鍵：`macroexpand-1`

看 macro 到底展成什麼——這是寫 macro 時最常用的工具：

```lisp
(macroexpand-1 '(my-when x (foo) (bar)))
; => (IF X (PROGN (FOO) (BAR)))

(macroexpand-1 '(swap a b))
; => (LET ((G111 A)) (SETF A B B G111))
```

`macroexpand-1` 只展一層；`macroexpand` 展到不能再展。在 nvim REPL 裡對著 macro 呼叫求值它，
一眼看出對不對。

**這招不只用在自己寫的 macro。** CL 內建那些看起來像魔法的 macro——最惡名昭彰的就是 `loop`——
一樣可以當場展開來看它到底做了什麼：

```lisp
(macroexpand-1 '(loop for x in '(1 2 3) collect (* x x)))
; => 一大坨 (block nil (let ...) (tagbody ...) ...) 的基本形式
```

意思是：Lisp 裡的抽象**永遠不是黑箱**。別的語言碰到看不懂的語法只能去翻文件、猜編譯器怎麼處理；
在 Lisp 你直接 `macroexpand` 把它攤回 `let` / `if` / `tagbody` 這些最基本的東西，自己看。
這也是「用 macro 造 DSL」在 CL 比在別的語言安全的原因——再花俏的 DSL，讀的人都能展開驗證。

## 什麼時候該用 macro

- 需要**控制求值**（延遲、不求值、改變順序）：`my-when`、`unless`、自訂 `with-...`。
- 需要**新語法 / 消除樣板**。
- 其餘一律用**函式**——函式好測、好組合、好推理。macro 是最後手段。

可跑版：[`examples/macros.lisp`](../examples/macros.lisp)。

下一步：[08-conditions.md](08-conditions.md)。
