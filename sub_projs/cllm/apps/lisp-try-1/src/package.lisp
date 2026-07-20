;;;; 套件定義（CL 的 package 相當於命名空間）。
(defpackage :lisp-try-1
  (:use :cl)
  (:export :main
           :cli-command
           ;; 核心（可離線單元測試）
           :ask
           :classify-error
           :binding-ready-p
           :resolve-ask))
