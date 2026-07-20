# 05 · ASDF 與 Quicklisp

- **ASDF** = build 系統：定義一個 system（有哪些檔、依賴誰、怎麼載）。內建於 SBCL。
- **Quicklisp** = 套件管理器：下載別人的 library、`quickload` 進來。

## 專案結構

```
cl-lab/
├─ cl-lab.asd          ← 系統定義（相當於 project.janet）
├─ src/
│  ├─ package.lisp     ← 套件（命名空間）定義
│  └─ main.lisp        ← 核心 + CLI
├─ tests/main.lisp     ← 測試（fiveam）
├─ scripts/run.sh      ← run/test/build/repl/swank
├─ examples/           ← 可跑範例
└─ docs/               ← 教學
```

## cl-lab.asd 拆解

```lisp
(asdf:defsystem "cl-lab"
  :version "0.1.0"
  :depends-on ("com.inuoe.jzon" "clingon")   ; 依賴，quickload 時自動抓
  :serial t                                   ; components 依序載入
  :components ((:module "src"
                :components ((:file "package") ; 不寫 .lisp 副檔名
                             (:file "main"))))
  :build-operation "program-op"               ; 這三行讓 asdf:make 能編出執行檔
  :build-pathname "build/cl-lab"
  :entry-point "cl-lab:main")
```

`:serial t` 表示按順序載入（package 要先於 main）。若要精確控制，可改用每個 file 的 `:depends-on`。

## 載入這個專案

因為 `~/quicklisp/local-projects/cl-lab` → 專案目錄的符號連結已建好，任何地方都能：

```lisp
(ql:quickload :cl-lab)         ; 連依賴一起抓好、載入
(cl-lab:hello "world")         ; => "Hello, world!"
```

## 日常指令（`scripts/run.sh`）

```sh
scripts/run.sh run -n Alice --json   # 跑 CLI（改→跑，開發主循環）
scripts/run.sh test                  # 跑測試（fiveam）
scripts/run.sh build                 # asdf:make → build/cl-lab（獨立執行檔，~45MB）
scripts/run.sh repl                  # 開已載入 cl-lab 的 REPL
```

`build` 用 `save-lisp-and-die` 把整個 image（含 SBCL runtime + 你的碼 + 依賴）存成一個檔，
**可直接複製到別台跑，不需對方裝 SBCL**。

## 開發時多半不用 build

CL 的循環是「REPL 一直開，改一個 `defun` 就重新求值它」（見 [06](06-編輯器與-repl.md)），
幾乎不需要 restart。`build` 只在要**交付一個執行檔**時才用。

## 裝新的庫

```lisp
(ql:quickload :some-library)         ; 從 Quicklisp dist 下載並載入
(ql:system-apropos "json")           ; 搜尋有哪些相關 system
```

要用在專案裡：把名字加進 `cl-lab.asd` 的 `:depends-on`，下次 `quickload :cl-lab` 就會帶上。

## 本地開發別人的庫

丟進 `~/quicklisp/local-projects/`（或做符號連結，像本專案那樣），`quickload` 就找得到，
會**優先於**線上版本。

下一步：[06-編輯器與-repl.md](06-編輯器與-repl.md)。
