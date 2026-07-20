;;;; lisp-try-1 的 ASDF 系統定義（相當於 janet 的 project.janet）。
;;;; 一支用 Common Lisp（SBCL）寫、透過 cllm 打 Anthropic API 的最小應用，
;;;; 整合 cllm 的兩支周邊工具：anthropic-proxy（轉發代理）+ llm-login（帳號登入）。
(asdf:defsystem "lisp-try-1"
  :description "用 Common Lisp 打 Anthropic API 的最小 cllm 應用（cllm binding → anthropic-proxy / OpenRouter）"
  :version "0.1.0"
  :author "lorkhan"
  :license "MIT"
  ;; 依賴：只用 clingon 做 CLI。cllm 的 CL binding 不進 ASDF——它是一支獨立檔，
  ;; 由 CLLM_LISP 環境變數定位、在執行期 (load) 進來（見 src/app.lisp resolve-ask）。
  :depends-on ("clingon")
  :serial t
  :components ((:module "src"
                :components ((:file "package")
                             (:file "app")
                             (:file "main"))))
  ;; asdf:make 會用這三行編出獨立執行檔 build/lisp-try-1
  :build-operation "program-op"
  :build-pathname "build/lisp-try-1"
  :entry-point "lisp-try-1:main"
  :in-order-to ((test-op (test-op "lisp-try-1/tests"))))

;;;; 測試系統：(asdf:test-system :lisp-try-1) 或 (ql:quickload :lisp-try-1/tests)
(asdf:defsystem "lisp-try-1/tests"
  :description "lisp-try-1 的離線測（不觸網、不需 binding）"
  :depends-on ("lisp-try-1" "fiveam")
  :serial t
  :components ((:module "tests"
                :components ((:file "main"))))
  :perform (asdf:test-op (o c)
             (uiop:symbol-call :fiveam :run! (uiop:find-symbol* :all-tests :lisp-try-1/tests))))
