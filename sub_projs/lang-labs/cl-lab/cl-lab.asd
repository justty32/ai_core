;;;; cl-lab 的 ASDF 系統定義（相當於 janet 的 project.janet / Python 的 pyproject）
(asdf:defsystem "cl-lab"
  :description "Common Lisp 開發環境試驗場 / dev sandbox"
  :version "0.1.0"
  :author "lorkhan"
  :license "MIT"
  ;; 依賴：改這裡再 (ql:quickload :cl-lab) 就會自動抓
  :depends-on ("com.inuoe.jzon"   ; JSON
               "clingon")          ; CLI（含子命令）
  :serial t                        ; 依序載入 components
  :components ((:module "src"
                :components ((:file "package")
                             (:file "main"))))
  ;; asdf:make 會用這三行編出獨立執行檔 build/cl-lab
  :build-operation "program-op"
  :build-pathname "build/cl-lab"
  :entry-point "cl-lab:main"
  :in-order-to ((test-op (test-op "cl-lab/tests"))))

;;;; 測試系統：(asdf:test-system :cl-lab) 或 (ql:quickload :cl-lab/tests)
(asdf:defsystem "cl-lab/tests"
  :description "cl-lab 的測試"
  :depends-on ("cl-lab" "fiveam")
  :serial t
  :components ((:module "tests"
                :components ((:file "main"))))
  :perform (asdf:test-op (o c)
             (uiop:symbol-call :fiveam :run! (uiop:find-symbol* :all-tests :cl-lab/tests))))
