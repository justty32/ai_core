# 03 · JSON（com.inuoe.jzon）

`com.inuoe.jzon`（簡稱 jzon）是現代 CL 的 JSON 首選：快、正確、API 乾淨。已裝好。

```lisp
(ql:quickload :com.inuoe.jzon)
;; 本專案的套件用 local-nickname (:json :com.inuoe.jzon)，可寫 json:stringify
```

## encode：`stringify`

```lisp
(com.inuoe.jzon:stringify #(1 2 3))            ; => "[1,2,3]"
(com.inuoe.jzon:stringify (list 1 2 3))        ; => "[1,2,3]"   list 也變 array
(com.inuoe.jzon:stringify h :pretty t)         ; :pretty t → 縮排好讀
```

**CL → JSON 的對應**：

| CL | JSON |
|----|------|
| hash-table（`equal`，字串 key） | object `{}` |
| list **或** vector（非空） | array `[]` |
| string / 數字 | 同名 |
| `t` | `true` |
| `nil` | `false` ⚠ 見下 |

> **`nil` 的陷阱**：CL 的 `nil` 同時是「假」「空串列」。jzon 把 `nil` 一律當 **`false`**（把 `t`
> 當 `true`——布林自然對應）。所以**空陣列不能用 `nil`**（會變成 `false`），要用**空向量 `#()`**；
> 空物件用**空 hash-table**：
>
> ```lisp
> (com.inuoe.jzon:stringify nil)                 ; => "false"   （不是 []！）
> (com.inuoe.jzon:stringify t)                   ; => "true"
> (com.inuoe.jzon:stringify #())                 ; => "[]"      空陣列這樣寫
> (com.inuoe.jzon:stringify (make-hash-table))   ; => "{}"      空物件
> ```
>
> JSON 的 `null` 解析後是一個專屬的 null 符號（會原樣 round-trip 回 `null`），實務上很少需要手寫。

要做 JSON **object** 一定要用 hash-table（不是 alist/plist）：

```lisp
(let ((h (make-hash-table :test 'equal)))
  (setf (gethash "name" h) "Bob"
        (gethash "n" h) 3)
  (com.inuoe.jzon:stringify h :pretty t))
; {
;   "name": "Bob",
;   "n": 3
; }
```

巢狀就是 hash-table 裝 hash-table / vector：

```lisp
(let ((meta (make-hash-table :test 'equal))
      (top  (make-hash-table :test 'equal)))
  (setf (gethash "active" meta) t
        (gethash "score" meta) 42
        (gethash "user" top) "Bob"
        (gethash "meta" top) meta)
  (com.inuoe.jzon:stringify top :pretty t))
; { "user": "Bob", "meta": { "active": true, "score": 42 } }
```

## decode：`parse`

```lisp
(com.inuoe.jzon:parse "{\"name\":\"Bob\"}")   ; => #<HASH-TABLE ...>  （object → hash-table）
(com.inuoe.jzon:parse "[10,20,30]")           ; => #(10 20 30)        （array → vector！）
```

**記住兩件事**：
- JSON object → hash-table（`test = equal`，key 是字串）：取值 `(gethash "name" h)`。
- JSON array → **vector**（不是 list）：取值 `(aref v i)`，走訪用 `loop for x across v`。

巢狀取值：

```lisp
(let ((j (com.inuoe.jzon:parse "{\"a\":{\"b\":[10,20,30]}}")))
  (aref (gethash "b" (gethash "a" j)) 1))      ; => 20
```

## 檔案 round-trip

```lisp
;; 寫
(with-open-file (s "cfg.json" :direction :output :if-exists :supersede)
  (write-string (com.inuoe.jzon:stringify h :pretty t) s))
;; 讀（jzon 也能直接吃 pathname / stream）
(com.inuoe.jzon:parse #p"cfg.json")
```

## 小抄

| 想做 | 怎麼寫 |
|------|--------|
| 資料 → 緊湊 JSON | `(json:stringify x)` |
| 資料 → 縮排 JSON | `(json:stringify x :pretty t)` |
| JSON → 資料 | `(json:parse s)` |
| 取 object 欄位 | `(gethash "key" ht)` |
| 取 array 元素 | `(aref vec i)` |
| 走訪 array | `(loop for x across vec ...)` |
| JSON false / null | `json:false` / `json:null`（別用 `nil`） |

下一步：[04-cli-clingon.md](04-cli-clingon.md)。
