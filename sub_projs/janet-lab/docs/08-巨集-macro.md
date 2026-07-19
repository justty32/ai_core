# 08 · 巨集 macro

巨集在**編譯期**把程式碼轉成別的程式碼。因為 Janet 的程式就是資料（tuple / array），巨集其實
就是「吃 AST、吐 AST」的函式。

## 四個符號先記住

| 符號 | 名稱 | 作用 |
|------|------|------|
| `~` | quasiquote | 引用一段程式（不求值），但允許裡面挖洞 |
| `,` | unquote | 在 `~` 裡「挖洞」：這裡的東西**要**求值後填進來 |
| `,;` | unquote-splice | 挖洞並**攤平**一個序列進來 |
| `'` | quote | 純引用，整段都不求值 |

## 定義巨集

```janet
(defmacro my-when [c & body]
  ~(if ,c (do ,;body)))            # 展開成 (if c (do body...))

(my-when true (print "hi"))        # => 印 hi
```

拆解 `~(if ,c (do ,;body))`：
- `~(...)` 我要生一段 `(if ...)` 程式碼。
- `,c` 把傳進來的條件 AST 填進去。
- `,;body` 把 `body`（一串運算式）攤平塞進 `(do ...)`。

```janet
(defmacro unless2 [c & body] ~(if ,c nil (do ,;body)))
(unless2 false (print "run"))      # => 印 run
```

## 看巨集展開成什麼（除錯必備）

```janet
(macex1 '(my-when x (foo) (bar)))
# => (if x (do (foo) (bar)))       # macex1 展開「一層」
(macex  '(my-when x (foo)))        # 展開到底（巢狀巨集都展開）
```

寫巨集卡住時，第一件事就是 `macex1` 看它到底生出什麼。

## 衛生：別讓臨時變數撞名

巨集裡如果自己 `let` 了一個變數，可能跟使用者的變數撞名。用 `with-syms` 生**保證不撞**的名字：

```janet
(defmacro swap [a b]
  (with-syms [tmp]                 # tmp 是一個 gensym，全域唯一
    ~(let [,tmp ,a] (set ,a ,b) (set ,b ,tmp))))

(var x 1) (var y 2)
(swap x y)                         # x=2 y=1
```

沒有 `with-syms` 而直接寫 `~(let [tmp ,a] ...)` 的話，若使用者剛好也有個叫 `tmp` 的變數就會出錯。
`with-syms` 是巨集正確性的標配。

## 什麼時候該用巨集

- **需要控制求值時機**：像 `my-when`、`unless`、`and`/`or` 短路——參數不能先被求值。
- **想造新語法 / DSL**：`for`、`loop`、`with`（自動清理資源）都是巨集。

反過來，**能用普通函式就別用巨集**——函式好測、好組合、能當值傳。巨集只在「函式做不到」
（要延遲求值、要看到未求值的程式結構）時才出手。

下一步：[09-fiber.md](09-fiber.md)。
