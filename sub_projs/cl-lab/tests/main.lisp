;;;; 測試（fiveam）。跑法：scripts/run.sh test 或 (asdf:test-system :cl-lab)
(defpackage :cl-lab/tests
  (:use :cl :fiveam)
  (:local-nicknames (:json :com.inuoe.jzon))
  (:export :all-tests))
(in-package :cl-lab/tests)

(def-suite all-tests :description "cl-lab 全部測試")
(in-suite all-tests)

(test hello
  (is (string= "Hello, world!" (cl-lab:hello)))
  (is (string= "Hello, Janet!" (cl-lab:hello "Janet"))))

(test greetings
  (is (equal '("Hello, a!" "Hello, b!") (cl-lab:greetings '("a" "b")))))

(test summarize
  (let ((s (cl-lab:summarize '("Alice" "Bob"))))
    (is (= 2 (gethash "count" s)))
    (is (equal '("Alice" "Bob") (gethash "names" s))))
  (let ((s (cl-lab:summarize '("x") t)))
    (is (string= "X" (first (gethash "names" s))))))

(test json-round-trip
  ;; jzon 編出來再 parse 回來，count 應為 1
  (let* ((str (cl-lab:summary-json '("Zed")))
         (back (json:parse str)))
    (is (= 1 (gethash "count" back)))
    (is (string= "Zed" (aref (gethash "names" back) 0)))))
