# 04 · CLI 參數（clingon）

`clingon` 是現代 CL 的命令列框架，**原生支援子命令**（正是你要的 `git submodule add` 那種）。
自動生成 `--help` 與 `--version`。

```lisp
(ql:quickload :clingon)
```

## 基本：一個命令 + 選項

```lisp
(defun greet-handler (cmd)
  (format t "names=~a upper=~a json=~a args=~a~%"
          (clingon:getopt cmd :name)             ; 讀選項值（用 :key）
          (clingon:getopt cmd :upper)
          (clingon:getopt cmd :json)
          (clingon:command-arguments cmd)))       ; 位置參數（非選項）

(defun cmd ()
  (clingon:make-command
   :name "greet" :description "打招呼"
   :options
   (list (clingon:make-option :list :description "名字，可重複給"
           :long-name "name" :short-name #\n :key :name)
         (clingon:make-option :flag :description "大寫"
           :long-name "upper" :short-name #\u :key :upper)
         (clingon:make-option :flag :description "JSON 輸出"
           :long-name "json" :short-name #\j :key :json))
   :handler #'greet-handler))

(clingon:run (cmd))          ; 不給第二參 → 讀真正的命令列參數
```

跑 `greet -n Alice -n Bob --upper pos1`：

```
names=(Alice Bob) upper=T json=NIL args=(pos1)
```

> **兩個必填**：`make-command` 要 `:description`，**每個 `make-option` 也要 `:description`**
> （少了會報 "Must specify description"）。

## 選項種類（`make-option` 第一個參數）

| kind | 意思 | 取出來 |
|------|------|--------|
| `:string` | 帶一個字串值 | 字串 |
| `:integer` | 帶一個整數 | 整數 |
| `:flag` | 開關 | `t` / `nil` |
| `:list` | 可重複給、收集成串列 | 串列 |
| `:counter` | 可重複、數次數（`-vvv`） | 整數 |

常用 key：`:long-name "x"`（`--x`）、`:short-name #\x`（`-x`）、`:key :x`（getopt 用的鍵）、
`:initial-value ...`（預設值）、`:required t`。

## 子命令（git-submodule 風格）

每個子命令自己是一個 `make-command`，掛到頂層的 `:sub-commands`：

```lisp
(defun add-command ()
  (clingon:make-command
   :name "add" :description "新增一個 submodule"
   :options (list (clingon:make-option :string :description "本地名稱"
                    :long-name "name" :short-name #\n :key :name)
                  (clingon:make-option :string :description "分支"
                    :long-name "branch" :short-name #\b
                    :initial-value "main" :key :branch))
   :handler (lambda (cmd)
              (format t "ADD url=~a name=~a branch=~a~%"
                      (first (clingon:command-arguments cmd))
                      (clingon:getopt cmd :name)
                      (clingon:getopt cmd :branch)))))

(defun top-command ()
  (clingon:make-command
   :name "tool" :description "示範"
   :sub-commands (list (add-command) (list-command))))

(clingon:run (top-command))
```

完整可跑版在 [`examples/subcommands.lisp`](../examples/subcommands.lisp)：

```sh
sbcl --script examples/subcommands.lisp add https://x.git --name libs -b dev
#   ADD url=https://x.git name=libs branch=dev
sbcl --script examples/subcommands.lisp list -v          # LIST verbose=T
sbcl --script examples/subcommands.lisp --help           # 自動列出 COMMANDS
```

**每個子命令各有自己的 `--help`、自己的選項集**。要更深一層（`git submodule add`）就是把「一個
有子命令的 make-command」再當成另一層的子命令掛上去——clingon 支援任意巢狀。

> `clingon:run` 跑完會結束行程（它就是設計成 CLI 進入點）。所以本專案 `src/main.lisp` 的
> `main` 直接 `(clingon:run (cli-command))` 即可。

下一步：[05-asdf-quicklisp.md](05-asdf-quicklisp.md)。
