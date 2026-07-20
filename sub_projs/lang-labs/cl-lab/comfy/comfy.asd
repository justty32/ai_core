;;;; comfy.asd — ASDF 系統定義（ASDF 隨 SBCL 內建，零外部相依）
;;;;
;;;; 載入： (asdf:load-system :comfy)   或在 Alive REPL 裡 Load System。
;;;; 測試： sbcl --script test/run.lisp （cwd 需在本資料夾）

(asdf:defsystem "comfy"
  :description "舒適 Common Lisp 地基：true/false 常數 + 'a' 字元讀取器 + Alive/VSCode debug 環境"
  :author "guanyu.lu"
  :license "（未定）"
  :version "0.1.0"
  :serial t
  :components ((:module "src"
                :serial t
                :components ((:file "package")
                             (:file "comfy")))))
