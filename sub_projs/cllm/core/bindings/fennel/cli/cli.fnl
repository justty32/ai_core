;; cli.fnl — 薄 CLI 外殼：把命令列組成一次 llm.ask 的發問（鏡像 C++ 的 `llm` CLI）。
;;
;; 只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給 binding 的 llm.ask。周邊
;; 拆到同資料夾的姊妹模組（皆對齊 C++ 的分檔）：internal（cli_internal.hpp）＝退出碼／env 鍵／檔案讀
;; 取；flags（cli_flags.cpp）＝反射旗標表＋print-usage；argv＝argv 掃描（cli.cpp 解析段）；config
;; （cli_config.cpp）＝三層 config；media＝--media 三分流；reqinput＝請求類旗標組輸入（cli.cpp 組請
;; 求段）；output（cli_output.cpp）＝出口 Sink。本檔（對齊 cli.cpp）只留 main 接線。
;;
;; 流程：(1) 掃描 argv → (2) 定 prompt（位置參數與導管 stdin 可合體；「-」＝插入點；互動終端不讀
;; stdin）→ (3) 組 client 設定（內建預設 → config 檔 → 反射旗標覆寫）→ (4) 組請求輸入＋呼叫 llm.ask
;; ，output 走 Sink。退出碼：0 成功／1 用法錯／2 請求失敗（含媒體落檔失敗）。

(local llm (require :llm))
(local internal (require :internal))
(local flags (require :flags))
(local argv (require :argv))
(local config (require :config))
(local reqinput (require :reqinput))
(local output (require :output))

(fn tty? []
  "stdin 是否互動終端（POSIX：無 stdlib isatty，靠 shell test -t 0；非 tty 才讀 stdin，避免卡住）。"
  (let [(ok _ code) (os.execute "test -t 0")]
    (and ok (= code 0))))

(fn assemble-prompt [p]
  "位置參數與導管 stdin 合體，回 (prompt nil) 或 (nil 退出碼)。"
  (let [has-dash (accumulate [d false _ part (ipairs p.prompt-parts)] (or d (= part "-")))]
    (if (and (tty?) has-dash)
        (do (io.stderr:write "「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（cllm --help 看用法）\n")
            (values nil internal.EXIT_USAGE))
        (let [stdin-text (if (tty?) "" (pick-values 1 (: (or (io.read "*a") "") :gsub "[\r\n]+$" "")))
              pieces (icollect [_ part (ipairs p.prompt-parts)] (if (= part "-") stdin-text part))
              joined (table.concat pieces " ")
              prompt (if (and (not has-dash) (not= stdin-text ""))
                         (if (= joined "") stdin-text (.. joined "\n\n" stdin-text))
                         joined)]
          (if (= prompt "")
              (do (io.stderr:write "缺少 prompt：給位置參數或從 stdin 餵入（cllm --help 看用法）\n")
                  (values nil internal.EXIT_USAGE))
              (values prompt nil))))))

(fn apply-flag-overrides [client p]
  "命令列反射旗標覆寫 client（型別轉換）；回 nil 或退出碼（型別錯）。"
  (var bad nil)
  (each [field entry (pairs p.raw-values)]
    (when (= bad nil)
      (let [[raw typ flag] entry]
        (if (= typ "str") (tset client field raw)
            (let [num (tonumber raw)]
              (if (= num nil)
                  (do (io.stderr:write (.. flag " 需要" (if (= typ "int") "整數" "小數") "（給了「" raw "」）\n"))
                      (set bad internal.EXIT_USAGE))
                  (tset client field num)))))))
  bad)

(fn build-opts [client prompt p r sink]
  "把 client 設定＋prompt＋請求輸入＋sink 回呼組成 llm.ask 的單一 opts table（底線鍵）。"
  (local opts {})
  (each [k v (pairs client)] (tset opts k v))
  (set opts.prompt prompt)
  (set opts.stream p.stream)
  (when p.has-system (set opts.system p.system-text))
  (when r.schema-body (set opts.schema r.schema-body))
  (when (> (length r.tool-defs) 0) (set opts.tools r.tool-defs))
  (when (> (length r.media-items) 0) (set opts.media r.media-items))
  (when (> (length r.modality-items) 0) (set opts.modalities r.modality-items))
  (set opts.on_delta sink.on_delta)
  (set opts.on_tool sink.on_tool)
  (set opts.on_media sink.on_media)
  (set opts.on_error sink.on_error)
  opts)

(fn main [args]
  "CLI 進入點；回退出碼。"
  (let [(p pec) (argv.parse-argv args)]
    (if (= p nil) pec ; --help（EXIT_OK）或用法錯
        (let [(prompt uec) (assemble-prompt p)]
          (if (= prompt nil) uec
              (let [client {}
                    cec (config.load-config client p.has-config p.config-path)]
                (if (not= cec internal.EXIT_OK) cec
                    (let [ovc (apply-flag-overrides client p)]
                      (if ovc ovc
                          (let [(r rec) (reqinput.build-request-inputs p)]
                            (if (= r nil) rec
                                (let [sink (output.make-sink p.media-out-dir)]
                                  (llm.ask (build-opts client prompt p r sink))
                                  (let [ok (not sink.had-error)]
                                    (when (and sink.wrote-text ok) (io.write "\n") (io.flush))
                                    (if (not ok) internal.EXIT_REQUEST
                                        sink.media-err internal.EXIT_REQUEST
                                        internal.EXIT_OK))))))))))))))

{: main}
