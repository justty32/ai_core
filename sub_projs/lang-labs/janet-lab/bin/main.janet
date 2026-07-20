#!/usr/bin/env janet
# CLI 進入點 —— 示範 spork/argparse 解析命令列參數。
# 跑法：
#   janet bin/main.janet -n Alice -n Bob --json      # 直接跑
#   janet bin/main.janet 二次元 --upper              # 位置參數也吃
#   jpm build && ./build/janet-lab --help            # 編成單一執行檔
(import spork/argparse :as ap)
(import ../janet-lab/init :as lab)

(defn main
  [& _]
  (def res
    (ap/argparse
      "janet-lab —— 打招呼 + JSON 小示範"
      # 具名選項可累積（--name/-n 可給多次）→ 收進一個陣列
      "name"  {:kind :accumulate :short "n" :help "要打招呼的名字，可重複給。"}
      "upper" {:kind :flag       :short "u" :help "名字轉大寫。"}
      "json"  {:kind :flag       :short "j" :help "以 JSON 輸出。"}
      # 特殊鍵 :default 收集不帶 -- 的位置參數
      :default {:kind :accumulate :help "位置參數也會被當成名字。"}))

  # argparse 解析失敗（或 --help）時回傳 nil，usage 已自動印出
  (unless res (os/exit 1))

  # 合併 --name 與位置參數；陣列示範 (@[...])
  (def names
    (let [collected @[]]
      (each n (or (res "name") @[]) (array/push collected n))
      (each n (or (res :default) @[]) (array/push collected n))
      (if (empty? collected) @["world"] collected)))

  (def upper? (res "upper"))

  (if (res "json")
    (print (lab/summary-json names upper?))
    (do
      # 雜湊表示範：直接讀 summarize 回來的 table
      (def s (lab/summarize names upper?))
      (printf "共 %d 位：" (s :count))
      (each g (s :hellos) (print "  " g)))))
