;;;; 套件定義。CL 的 package 相當於命名空間。
(defpackage :cl-lab
  (:use :cl)
  ;; local-nickname：在本套件裡用 json: 代替冗長的 com.inuoe.jzon:
  (:local-nicknames (:json :com.inuoe.jzon))
  (:export :main
           :cli-command
           :hello
           :greetings
           :summarize
           :summary-json))
