# 06 · 編輯器與 REPL

Common Lisp 的靈魂是**互動式開發**：REPL 一直開，程式在「活著」的狀態下一塊一塊長出來。
你不是「寫完再跑」，而是「邊跑邊改」。

## nvim 已配好（Conjure + Swank）

跟你 scheme/janet 同一套 Conjure，但 CL 走 **Swank**（SLIME 的後端），是 **connect 模式**：
先在外面跑一個 swank 伺服器，nvim 再連上去。

設定檔：`~/.config/nvim/lua/plugins/cl.lua`（filetype `lisp`）。parinfer + 彩虹括號本來就涵蓋 `lisp`。

### 工作流

```sh
# 1) 開一個 terminal 起 swank（會順便載入 :cl-lab）
cd ~/code/cl-lab && scripts/run.sh swank        # 監聽 127.0.0.1:4005
```

```
# 2) nvim 開任何 .lisp 檔
,cc     連上 swank（connect）
,ee     求值游標所在的 form
,er     求值最外層 form
,eb     求值整個 buffer
,ls / ,lv   開 REPL log（橫 / 直）
,cd     斷線（disconnect）
```

寫一個 `defun` 就 `,ee` 送進去，函式立刻在活著的 image 裡更新，馬上能用下一個 form 呼叫它試。

## 純 REPL（不開編輯器也行）

```sh
scripts/run.sh repl          # 開一個已 (ql:quickload :cl-lab) 的 SBCL REPL
```

REPL 裡最有用的幾招：

```lisp
(cl-lab:hello "world")            ; 直接呼叫
(describe 'cl-lab:summarize)      ; 看某個東西是什麼（函式/變數/類別…）
(documentation 'cl-lab:hello 'function)  ; 看 docstring
(apropos "stringify")             ; 模糊搜尋符號名
(inspect *h*)                     ; 互動式檢視一個物件
*                                 ; 上一個結果；** 上上個；*** 再上一個
```

### 出錯時會掉進 debugger

REPL 裡求值出錯，SBCL 會進**互動式 debugger**：列出可用的 **restart**（例如 retry、
用別的值繼續、abort）。這不是壞事——它讓你在錯誤現場檢查、修好、繼續，不必從頭跑。
選 `0`（通常是 abort）回到 REPL。詳見 [08-conditions.md](08-conditions.md)。

（在 `--script` / `--disable-debugger` 模式下則不會進 debugger，直接印 backtrace 退出——
所以那個模式適合批次腳本，不適合互動。）

## 別把「有 REPL」當賣點

這裡要講清楚一個常被誤會的點：**光是「有一個 REPL」不稀奇**，Python、Ruby、Node 都有。
CL 互動式開發真正強的地方是兩件事：

1. **編輯器直接連上一個活著的行程**，在原本的情境裡求值一小段 form（就是上面 `,ee` 在做的），
   不是另開一個乾淨的 CLI 從頭跑。
2. **連「類別」都能熱改**——不只函式。這靠的是 CLOS（CL 的物件系統）。

第二點值得親手試一次。開 `repl`，定一個類別、造一個實例：

```lisp
(defclass point () ((x :initarg :x :accessor px)))
(defparameter *p* (make-instance 'point :x 1))
(px *p*)                                  ; => 1
```

現在**在不重啟、不重造 `*p*` 的情況下**，改類別定義、加一個欄位 `y`：

```lisp
(defclass point ()
  ((x :initarg :x :accessor px)
   (y :initarg :y :initform 0 :accessor py)))   ; 新增 y，預設 0

(py *p*)                                  ; => 0  ← 舊實例自動補上了新欄位
(px *p*)                                  ; => 1  ← 舊資料還在
```

那個早就存在的 `*p*` 會**沿用**新的類別定義、把新欄位補成 initform 值——這叫
`update-instance-for-redefined-class`，CL 幫你自動做。要把一個實例整個換成別的類別，還有 `change-class`。

換句話說：程式跑到一半、堆了一坨實驗狀態，你能一邊改設計一邊保住那些狀態，不必推倒重來。
這才是「活系統」跟「有個 REPL 可以打指令」的差別。

## 心法

- **不要 restart SBCL 來套用改動**——重新求值那個 `defun` 就好，image 是活的。類別也一樣：改
  `defclass` 重新求值，早就造好的實例會自動跟上（見上節 CLOS）。
- 資料也活著：`(defparameter *x* ...)` 放一份實驗資料，反覆對它試函式。
- 卡住看不懂某東西：`describe` / `inspect` / `apropos` 三件套。

下一步：[07-macro.md](07-macro.md)。
