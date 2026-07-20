;; argv.fnl — 命令列掃描：把 args 拆成旗標與位置參數（對齊 cli.cpp 的解析段）。
;;
;; 固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
;; 特判；反射旗標（連線／取樣，對應 Client 欄位，見 flags.fnl）吃下一個 argv；其餘當位置參數拼
;; prompt。「-」單獨＝stdin 插入點，其餘 '-' 開頭＝未知旗標。掃描結果收成一個 table；遇 --help 或
;; 用法錯時回 (nil 退出碼) 交呼叫端直接返回（main 只讀結果、不重演解析）。

(local flags (require :flags))
(local internal (require :internal))

(fn new-parsed []
  {:raw-values {} :prompt-parts [] :media-specs [] :tool-specs [] :modality-specs []
   :schema-text nil :config-path nil :media-out-dir nil :system-text nil
   :has-schema false :has-config false :has-system false :stream false})

(fn parse-argv [args]
  "掃描 args（1-based 序列，不含程式名），回 (parsed nil)；遇 --help／用法錯回 (nil 退出碼)。"
  (local p (new-parsed))
  (local n (length args))
  (var i 1)
  (var result nil) ; 非 nil＝提前收工的 [值 退出碼]
  (var no-more-flags false)
  (fn need-value [flag]
    "吃下一個 argv 當旗標的值；缺值回 nil（呼叫端據此回 EXIT_USAGE）。"
    (if (> (+ i 1) n)
        (do (io.stderr:write (.. flag " 缺少值（cllm --help 看用法）\n")) nil)
        (do (set i (+ i 1)) (. args i))))
  (while (and (<= i n) (= result nil))
    (let [a (. args i)]
      (if
        no-more-flags (do (table.insert p.prompt-parts a) (set i (+ i 1)))
        (= a "--") (do (set no-more-flags true) (set i (+ i 1)))
        (or (= a "--help") (= a "-h")) (do (flags.print-usage) (set result [nil internal.EXIT_OK]))
        (= a "--stream") (do (set p.stream true) (set i (+ i 1)))
        (or (= a "--image") (= a "--media"))
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (table.insert p.media-specs v) (set i (+ i 1)))))
        (= a "--schema")
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (set p.schema-text v) (set p.has-schema true) (set i (+ i 1)))))
        (= a "--system")
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (set p.system-text v) (set p.has-system true) (set i (+ i 1)))))
        (= a "--config")
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (set p.config-path v) (set p.has-config true) (set i (+ i 1)))))
        (= a "--tool")
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (table.insert p.tool-specs v) (set i (+ i 1)))))
        (= a "--modality")
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (table.insert p.modality-specs v) (set i (+ i 1)))))
        (= a "--media-out")
          (let [v (need-value a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (set p.media-out-dir v) (set i (+ i 1)))))
        (. flags.FLAG_TO_FIELD a)
          (let [v (need-value a) [field typ] (. flags.FLAG_TO_FIELD a)]
            (if (= v nil) (set result [nil internal.EXIT_USAGE])
                (do (tset p.raw-values field [v typ a]) (set i (+ i 1)))))
        (and (> (length a) 0) (= (a:sub 1 1) "-") (not= a "-")) ; 未知旗標
          (do (io.stderr:write (.. "未知旗標：" a "（cllm --help 看用法）\n"))
              (set result [nil internal.EXIT_USAGE]))
        (do (table.insert p.prompt-parts a) (set i (+ i 1))))))
  (if result (values (. result 1) (. result 2)) (values p nil)))

{: parse-argv}
