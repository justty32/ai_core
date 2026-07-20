# 08 · 條件系統（conditions / restarts）

這是 Common Lisp 最強大、也最與眾不同的一塊。比 try/catch 多一個維度：**錯誤「發生的地方」
與「怎麼處理」可以分離，而且處理時堆疊還沒被拆掉**——你可以選一個 restart，讓程式**從錯誤點
繼續**，而不是只能往上拋。

## 三層由淺入深

### 1) `handler-case`：最像 try/catch

```lisp
(define-condition my-error (error)               ; 自訂條件（像自訂 exception）
  ((msg :initarg :msg :reader msg)))

(handler-case (error 'my-error :msg "boom")
  (my-error (e) (format nil "抓到：~a" (msg e))))  ; => "抓到：boom"
```

`error` 是「signal 一個條件並要求處理」。`handler-case` 依型別配對 handler，處理完**整個
handler-case 回傳** handler 的值（堆疊已拆掉，跟其它語言一樣）。

### 2) `ignore-errors`：出錯就回 nil

```lisp
(ignore-errors (error "故意出錯"))
; => NIL, #<SIMPLE-ERROR ...>     （回兩個值：nil 和那個條件）
```

### 3) `restart-case` + `handler-bind`：CL 的殺手鐧

`restart-case` 在可能出錯的地方，**預先提供幾個「復原方案」（restart）**。呼叫端用
`handler-bind` 攔截條件，決定去 `invoke` 哪個 restart——**此時堆疊還在**，restart 讓執行
從 `restart-case` 那個點繼續。

```lisp
(defun safe-div (a b)
  (restart-case (if (zerop b) (error "除以零") (/ a b))
    (use-zero () 0)))                             ; 提供一個名為 use-zero 的復原方案

(handler-bind ((error (lambda (c)
                        (declare (ignore c))
                        (invoke-restart 'use-zero))))  ; 攔到 error → 走 use-zero
  (safe-div 1 0))                                  ; => 0

(safe-div 6 2)                                     ; => 3（正常路徑）
```

為什麼強：**做決策的層**（handler-bind，通常在高層、知道整體策略）和**知道怎麼復原的層**
（restart-case，在低層、知道局部細節）**解耦**了。低層只負責「我提供這些選項」，高層負責
「這情況選哪個」。這也是 SBCL 互動 debugger 的底層機制——出錯時它列的那些選項就是當下可用的
restart，你在 REPL 手動挑一個就能繼續。

## `unwind-protect`：像 finally

不管有沒有出錯，清理一定執行：

```lisp
(unwind-protect
     (do-work)                 ; body
  (cleanup))                   ; 保證跑（即使 body 出錯 / 非區域跳出）
```

## 條件不一定是錯誤

`define-condition` 也可以繼承 `warning` 或 `condition`（非 error），用 `signal` 發出，
當成一種「通知/事件」機制——不是只有出錯才用。

## 小抄

| 想做 | 用 |
|------|----|
| try/catch | `handler-case` |
| 出錯回 nil | `ignore-errors` |
| 提供復原點 | `restart-case` |
| 攔截並選 restart | `handler-bind` + `invoke-restart` |
| finally | `unwind-protect` |
| 自訂條件 | `define-condition` |

可跑版：[`examples/conditions.lisp`](../examples/conditions.lisp)。

下一步：[09-c-ffi.md](09-c-ffi.md)。
