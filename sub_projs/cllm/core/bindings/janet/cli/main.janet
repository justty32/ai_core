#!/usr/bin/env janet
# main.janet — 薄 CLI 外殼：把命令列組成一次 llm/ask 的發問（鏡像 C++ 的 `llm` CLI）。
#
# 只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給 binding 的 llm/ask。周邊
# 拆到同資料夾的姊妹模組（皆對齊 C++ 的分檔）：internal（cli_internal.hpp）＝退出碼／env 鍵／檔案
# 讀取；flags（cli_flags.cpp）＝反射旗標表＋print-usage；argv＝argv 掃描（cli.cpp 解析段）；config
# （cli_config.cpp）＝三層 config；media＝--media 三分流；reqinput＝請求類旗標組成 llm/ask 輸入；
# output（cli_output.cpp）＝出口 handlers（Sink）。本檔（對齊 cli.cpp）只留 main 接線。
#
# 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消
# （SIGINT 未裝 handler，交 OS 預設終止＝130；串流中途取消由 libcllm 內部處理、回已收部分）。

(import llm)
(import ./internal :as internal)
(import ./argv :as argv)
(import ./config :as config)
(import ./flags :as flags)
(import ./output :as output)
(import ./reqinput :as reqinput)

(defn- read-stdin []
  ``只在被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）；去尾端換行避免多餘空白進 prompt。``
  (if (os/isatty stdin) ""
    (string/trimr (string (or (file/read stdin :all) "")) "\r\n")))

(defn- add-opt [opts k v] (when (not (nil? v)) (array/push opts k) (array/push opts v)))

(defn run [args]
  ``CLI 主流程；回退出碼。``
  (label done
    # ── (1) 掃描 argv ──
    (def [p ec] (argv/parse-argv args))
    (when (nil? p) (return done ec))

    # ── (2) prompt：位置參數與導管 stdin 可合體 ──
    (def has-dash (some |(= $ "-") (p :prompt-parts)))
    (def piped (not (os/isatty stdin)))
    (def stdin-text (read-stdin))
    (when (and (not piped) has-dash)
      (eprint "「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（llm --help 看用法）")
      (return done internal/exit-usage))
    (def pieces (map |(if (= $ "-") stdin-text $) (p :prompt-parts)))
    (var prompt (string/join pieces " "))
    (when (and (not has-dash) (not (empty? stdin-text))) # 沒寫「-」而兩者都有＝prompt＋空行＋stdin
      (set prompt (if (empty? prompt) stdin-text (string prompt "\n\n" stdin-text))))
    (when (empty? prompt)
      (eprint "缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）")
      (return done internal/exit-usage))

    # ── (3) 組 client 設定：內建預設 → config → 反射旗標覆寫 ──
    (def client @{})
    (def cec (config/load-config client (p :has-config) (p :config-path)))
    (when (not= cec internal/exit-ok) (return done cec))
    (eachp [kw [val typ flag]] (p :raw-values)
      (if (= typ :string)
        (put client kw val)
        (let [num (scan-number val)]
          (if (nil? num)
            (do (eprintf "%s 需要數值（給了「%s」）" flag val) (return done internal/exit-usage))
            (put client kw (if (= typ :int) (math/trunc num) num))))))

    # ── (4) 組請求輸入 ＋ 呼叫 binding ──
    (def [r rec] (reqinput/build-request-inputs p))
    (when (nil? r) (return done rec))

    (def sink (output/make-sink (p :media-out-dir)))
    (def st (sink :state))
    (def opts @[])
    (each [flag ck kw typ] flags/reflect-flags
      (add-opt opts kw (get client kw)))
    (add-opt opts :system (when (p :has-system) (p :system-text)))
    (add-opt opts :schema (r :schema-body))
    (when (not (empty? (r :tool-defs))) (add-opt opts :tools (r :tool-defs)))
    (when (not (empty? (r :media-items))) (add-opt opts :media (r :media-items)))
    (when (not (empty? (r :modality-items))) (add-opt opts :modalities (r :modality-items)))
    (array/push opts :stream (p :stream))
    (array/push opts :on-delta (sink :on-delta))
    (array/push opts :on-tool (sink :on-tool))
    (array/push opts :on-media (sink :on-media))
    (array/push opts :on-error (sink :on-error))

    (llm/ask prompt ;opts)

    # ── 收尾：定退出碼 ──
    (def ok (not (st :had-error)))
    (when (and (st :wrote-text) ok) (prin "\n") (flush)) # 補尾端換行
    (cond
      (not ok) internal/exit-request
      (st :media-err) internal/exit-request # 媒體落檔失敗＝結果真掉了
      internal/exit-ok)))

(defn main [& _]
  (os/exit (run (dyn :args))))

(main)
