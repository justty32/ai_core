;;;; 子命令示範（clingon 原生支援，git-submodule 風格）。
;;;; 跑法：
;;;;   sbcl --script examples/subcommands.lisp add https://x.git --name libs -b dev
;;;;   sbcl --script examples/subcommands.lisp list -v
;;;;   sbcl --script examples/subcommands.lisp --help
;;;; 詳解見 docs/04-cli-clingon.md
(load (merge-pathnames "quicklisp/setup.lisp" (user-homedir-pathname)))
(ql:quickload :clingon :silent t)

;; 每個子命令：make-command + 自己的 options + handler
(defun add-handler (cmd)
  (format t "ADD url=~a name=~a branch=~a~%"
          (first (clingon:command-arguments cmd))
          (clingon:getopt cmd :name)
          (clingon:getopt cmd :branch)))

(defun list-handler (cmd)
  (format t "LIST verbose=~a~%" (clingon:getopt cmd :verbose)))

(defun add-command ()
  (clingon:make-command
   :name "add" :description "新增一個 submodule"
   :options (list (clingon:make-option :string :description "本地名稱"
                    :long-name "name" :short-name #\n :key :name)
                  (clingon:make-option :string :description "分支"
                    :long-name "branch" :short-name #\b
                    :initial-value "main" :key :branch))
   :handler #'add-handler))

(defun list-command ()
  (clingon:make-command
   :name "list" :description "列出所有 submodule"
   :options (list (clingon:make-option :flag :description "詳細"
                    :long-name "verbose" :short-name #\v :key :verbose))
   :handler #'list-handler))

;; 頂層命令：把子命令掛在 :sub-commands
(defun top-command ()
  (clingon:make-command
   :name "tool" :description "git 風格子命令示範"
   :sub-commands (list (add-command) (list-command))))

;; clingon:run 不給第二參時，讀真正的命令列參數
(clingon:run (top-command))

;; 巢狀（git submodule add）就是把一個「有子命令的 make-command」再當成
;; 另一個頂層的子命令掛上去——clingon 支援任意層數。
