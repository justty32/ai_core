(declare-project
  :name "janet-lab"
  :description "Janet 開發環境試驗場 / dev sandbox"
  :version "0.1.0"
  # 依賴宣告在這裡；改完跑 `jpm deps` 安裝
  :dependencies ["spork"])

(declare-source
  :prefix "janet-lab"
  :source ["janet-lab/init.janet"])

# 產生一個可執行檔 build/janet-lab（jpm build）
(declare-executable
  :name "janet-lab"
  :entry "bin/main.janet"
  :install false)
