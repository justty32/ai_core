# 09 · C FFI（cffi）

`cffi` 讓你**不寫任何 C、不編譯**，直接呼叫系統的共享庫（`.so`）。是 CL 生態的事實標準
（可攜到 SBCL/CCL/ECL 等多種實作）。

```lisp
(ql:quickload :cffi)
```

## 三步：載庫 → 宣告 → 呼叫

```lisp
;; 1) 載入共享庫
(cffi:load-foreign-library "libc.so.6")
(cffi:load-foreign-library "libm.so.6")

;; 2) defcfun：宣告外部函式的簽名
;;    (defcfun ("C名字" lisp名字) 回傳型別 (參數名 型別) ...)
(cffi:defcfun ("strlen" c-strlen) :unsigned-long
  (s :string))
(cffi:defcfun ("sqrt" c-sqrt) :double
  (x :double))

;; 3) 之後就像普通 Lisp 函式呼叫
(c-strlen "hello")     ; => 5
(c-sqrt 2.0d0)         ; => 1.4142135623730951d0
```

> 浮點數記得用 **double**（`2.0d0`，不是 `2.0`）對上 C 的 `double`；型別對不上會拿到垃圾值。

## 常用型別關鍵字

| cffi | C |
|------|---|
| `:void` | void |
| `:int` / `:unsigned-int` | int / unsigned |
| `:long` / `:unsigned-long` | long / size_t 常用 |
| `:float` / `:double` | float / double |
| `:string` | `char *`（自動轉，傳 CL 字串進去） |
| `:pointer` | 任意指標 |
| `:bool` | C99 bool |

## 更進階（用得到再查）

- `cffi:defcstruct` / `with-foreign-object`：對應 C struct、配置外部記憶體。
- `cffi:foreign-alloc` / `foreign-free`：手動管理外部記憶體。
- `cffi:mem-ref` / `mem-aref`：讀寫指標指向的值。
- `cffi:defcallback`：把 CL 函式當 C callback 傳出去。
- `cffi:define-foreign-library` + `use-foreign-library`：可攜地宣告「不同平台的庫檔名」。

## 完整可跑範例

[`examples/ffi-demo.lisp`](../examples/ffi-demo.lisp)：

```sh
sbcl --script examples/ffi-demo.lisp
# strlen("hello") = 5
# sqrt(2)         = 1.41421
# cos(0)          = 1.0000
```

## 和 Janet 那邊的差別

Janet 用內建 `ffi/*`（要手描 signature）；CL 用 cffi 的 `defcfun`（宣告式，較貼近 C header）。
兩者都是「不編譯、執行期綁定共享庫」。CL 這邊沒有「把直譯器嵌進 C」的對稱需求——因為開發本來
就在活著的 image + REPL 裡進行，需要 C 時把 C 拉進來（FFI）比把 Lisp 塞進 C 更常見。

回目錄：[README.md](README.md)。
