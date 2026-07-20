# 09 · Fiber（協程）

Fiber 是 Janet 的招牌。它是**一段可以暫停 / 恢復的執行流**，而且 Janet 用同一個機制實現了
generator、例外處理、以及非同步——三件事一套東西。

## 基本：yield 產值、resume 恢復

```janet
(def f (fiber/new (fn [] (yield 1) (yield 2) (yield 3))))
(resume f)   # => 1   跑到第一個 yield 就停、把值交出來
(resume f)   # => 2   從剛才停的地方繼續
(resume f)   # => 3
(fiber/status f)   # => :pending（還沒真正結束）
```

`fiber/new` 把一個函式包成 fiber；`resume` 推它往前跑到下一個 `yield`。這就是 generator。

## 當 generator 走訪

fiber 可以直接被 `each` / `loop :in` 走訪，把每個 `yield` 的值當成序列元素：

```janet
(def squares (fiber/new (fn [] (for i 0 5 (yield (* i i))))))
(each x squares (prin x " "))     # => 0 1 4 9 16
(loop [x :in squares] ...)        # 同義
```

好處：**惰性**。值是要用時才算出來，適合大 / 無限序列。

## 例外處理其實也是 fiber

`try` 就是包在 fiber 上的糖——底層是「用一個攔截 error 信號的 fiber 跑 body」：

```janet
(try
  (error "boom")
  ([err] (string "抓到：" err)))   # => "抓到：boom"
```

`protect` 更輕量，把結果包成 `(成功? 值)` 的 tuple、不 throw：

```janet
(protect (error "bad"))           # => (false "bad")
(protect (+ 1 2))                 # => (true 3)
```

（小提醒：`(/ 1 0)` 在 Janet 是 `inf` 不是錯誤，別拿它試 protect。）

## 手動攔信號（進階）

`fiber/new` 第二參是**信號遮罩**，決定這個 fiber 要攔哪些信號。`:e` = 攔 error：

```janet
(def g (fiber/new (fn [] (error "x")) :e))
(resume g)                        # => "x"（不會炸出來）
(fiber/status g)                  # => :error
```

常見遮罩字元：`:e` 攔 error、`:y` 攔 yield、`:a` 全攔。`try` / generator 本質上就是選了不同遮罩。

## 非同步 / 並發：ev

Janet 內建 `ev`——一個 fiber 排程器（event loop）。`ev/spawn` 丟一個 fiber 去背景跑，遇到
I/O 或 `ev/sleep` 就讓出、換別的 fiber，**單執行緒協作式並發**：

```janet
(ev/spawn (print "背景 A"))
(ev/spawn (print "背景 B"))
(ev/sleep 0)                      # 讓排程器有機會跑背景 fiber
(print "主線")
```

搭配 `ev/read`、`ev/write`、channel（`ev/chan`）可以寫非阻塞的網路 / IO 程式，而且**寫起來像
同步程式碼**——沒有 callback 地獄、沒有 async/await 染色。這是 fiber 相對其它語言協程的賣點。

完整可跑範例：[`examples/fibers.janet`](../examples/fibers.janet)。

下一步：[10-c-互通.md](10-c-互通.md)。
