# argv.janet — 命令列掃描：把 args 拆成旗標與位置參數（對齊 core/src/cli.cpp 的解析段）。
#
# 固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
# 特判；反射旗標（連線／取樣，對應 Client 欄位，見 flags.janet）吃下一個 arg；其餘當位置參數拼
# prompt。「-」單獨＝stdin 插入點，其餘 '-' 開頭＝未知旗標。掃描結果收成一個 table；遇 --help
# 或用法錯時回退出碼交呼叫端直接返回（main 只讀結果、不重演解析）。

(import ./internal :as internal)
(import ./flags :as flags)

(defn new-parsed []
  ``argv 掃描結果表：旗標收成的各欄位，交給 main.janet 續組 prompt／config／請求。``
  @{:raw-values @{}      # ask關鍵字 → [原始字串 型別 flag]
    :prompt-parts @[]
    :media-specs @[]
    :tool-specs @[]
    :modality-specs @[]
    :schema-text nil :config-path nil :media-out-dir nil :system-text nil
    :has-schema false :has-config false :has-system false :stream false})

(defn parse-argv [args]
  ``掃描 args（args[0]＝腳本名，從 1 起），回 [parsed nil]；遇 --help／用法錯回 [nil 退出碼]。``
  (def p (new-parsed))
  (def n (length args))
  (var i 1)
  (var no-more false)
  (var result nil) # [nil 退出碼]＝提早返回

  (defn need-value [flag] # 吃下一個 arg 當旗標的值；缺值回 nil（呼叫端據此回 exit-usage）
    (if (>= (+ i 1) n)
      (do (eprintf "%s 缺少值（llm --help 看用法）" flag) nil)
      (do (set i (+ i 1)) (get args i))))

  (defn take-listed [flag arr] # 需值→推進 arr；缺值設用法錯
    (def v (need-value flag))
    (if (nil? v) (set result [nil internal/exit-usage])
      (do (array/push arr v) (set i (inc i)))))

  (defn take-into [flag key flagkey] # 需值→寫欄位 key＝值、flagkey＝true
    (def v (need-value flag))
    (if (nil? v) (set result [nil internal/exit-usage])
      (do (put p key v) (when flagkey (put p flagkey true)) (set i (inc i)))))

  (while (and (< i n) (nil? result))
    (def a (get args i))
    (cond
      no-more (do (array/push (p :prompt-parts) a) (set i (inc i)))
      (= a "--") (do (set no-more true) (set i (inc i)))
      (or (= a "--help") (= a "-h")) (do (flags/print-usage) (set result [nil internal/exit-ok]))
      (= a "--stream") (do (put p :stream true) (set i (inc i)))
      (or (= a "--image") (= a "--media")) (take-listed a (p :media-specs))
      (= a "--tool") (take-listed a (p :tool-specs))
      (= a "--modality") (take-listed a (p :modality-specs))
      (= a "--schema") (take-into a :schema-text :has-schema)
      (= a "--system") (take-into a :system-text :has-system)
      (= a "--config") (take-into a :config-path :has-config)
      (= a "--media-out") (take-into a :media-out-dir nil)
      (get flags/flag->spec a) # 反射旗標：吃下一 arg，收進 raw-values（鍵＝ask關鍵字）
      (let [v (need-value a)]
        (if (nil? v) (set result [nil internal/exit-usage])
          (let [[ck kw typ] (get flags/flag->spec a)]
            (put (p :raw-values) kw [v typ a])
            (set i (inc i)))))
      (and (> (length a) 0) (= (get a 0) 45) (not= a "-")) # 「-」＝stdin 佔位；其餘 '-' 開頭＝未知旗標
      (do (eprintf "未知旗標：%s（llm --help 看用法）" a) (set result [nil internal/exit-usage]))
      (do (array/push (p :prompt-parts) a) (set i (inc i)))))

  (if result result [p nil]))
