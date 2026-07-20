;; util.fnl — hermy Fennel 版的「庫」：所有邏輯都在這裡，每個 CLI 只是「載入本檔＋呼叫
;; 一個 main」的薄殼（≤10 行）。本庫從自己被 require 時的第二個 vararg（自身檔路徑）算出
;; *root*（＝ src/ 的上一層＝專案根），與哪個 CLI 載它無關——Fennel/Lua 版的 *load-truename*。
;; HTTP 靠 curl、JSON 靠 jq，全部 shell-out。Lua 陷阱都在這裡解掉（見各 helper 註解）。
(local (_modname mypath) ...)   ; require 傳入 (模組名 檔路徑)

;; *root* = 本庫檔（.../src/util.fnl）的上兩層＝專案根，與哪個 CLI load 它無關。
(local srcdir (: mypath :match "^(.*)/[^/]*$"))          ; .../src
(local root (or (: srcdir :match "^(.*)/[^/]*$") "."))   ; 專案根

;; ── I/O 與環境小工具 ────────────────────────────────────────────────
(fn trim [s] (pick-values 1 (: (or s "") :gsub "^%s*(.-)%s*$" "%1")))  ; gsub 回兩值，pick 截一值

(fn env [name default]
  (let [v (os.getenv name)]
    (if (and v (not= v "")) v default)))

(fn slurp-stdin []
  (or (io.read "*a") ""))

(fn read-file [path]
  (let [f (io.open path "r")]
    (if f (let [s (f:read "*a")] (f:close) (or s "")) nil)))

(fn exists? [path]
  (let [f (io.open path "r")]
    (if f (do (f:close) true) false)))

(fn emit [s] (io.write (or s "") "\n") (io.flush))

(fn eprint [...] (io.stderr:write (table.concat [...] "") "\n"))

(fn die [code ...] (eprint ...) (os.exit code))

;; 自寫 shquote：把字串包成單引號，內部 ' 轉成 '\'' （Lua gsub 回兩值，pick 截一值）。
(fn shq [s] (.. "'" (pick-values 1 (: (tostring (or s "")) :gsub "'" "'\\''")) "'"))

;; 寫暫存檔（io.popen 是單向的，stdin 一律走暫存檔——Lua 陷阱）。回檔名。
(fn write-tmp [s]
  (let [t (os.tmpname) f (io.open t "w")] (f:write (or s "")) (f:close) t))

;; 執行命令、餵 stdin、抓 stdout/stderr/exit-code（io.popen 單向，故各接一暫存檔＋echo $?）。
(fn run [argv input]
  (let [inf (write-tmp (or input ""))
        of (os.tmpname) ef (os.tmpname) cf (os.tmpname)
        parts []]
    (each [_ a (ipairs argv)] (table.insert parts (shq a)))
    (os.execute (.. (table.concat parts " ") " < " inf " > " of " 2> " ef "; echo $? > " cf))
    (let [out (or (read-file of) "")
          errtxt (trim (or (read-file ef) ""))
          code (or (tonumber (trim (or (read-file cf) ""))) 0)]
      (each [_ p (ipairs [inf of ef cf])] (os.remove p))
      (values out errtxt code))))

;; jq -c <flags...> <filter>，input 走 stdin（大 JSON 避開 argv 上限）。jq 失敗回 nil。
(fn jq [filter input flags]
  (let [inf (write-tmp (or input "null"))
        cmd (.. "jq -c " (or flags "") " " (shq filter) " " inf " 2>/dev/null")
        p (io.popen cmd)
        o (or (p:read "*a") "")
        ok (p:close)
        _ (os.remove inf)
        out (pick-values 1 (: o :gsub "%s+$" ""))]
    (if (and ok (not= out "")) out nil)))

(fn iso-now [] (os.date "!%Y-%m-%dT%H:%M:%S+00:00"))

;; 目錄解析（env 覆寫 or 同層預設）
(fn skills-dir [] (or (env "HERMY_SKILLS_DIR") (.. root "/skills")))
(fn memory-dir [] (or (env "HERMY_MEMORY_DIR") (.. root "/memory")))

;; ── 編排器用的小助手 ────────────────────────────────────────────────
;; 呼叫 lib/<tool>：轉發其 stderr、非零退出即結束、回其 stdout。
(fn call [tool args input]
  (let [argv [(.. root "/lib/" tool)]]
    (each [_ a (ipairs (or args []))] (table.insert argv a))
    (let [(out err code) (run argv (or input ""))]
      (when (not= (trim err) "") (eprint (trim err)))
      (when (not= code 0) (os.exit code))
      out)))

(fn mem [rec] (call "mem-append" [] rec))
(fn skills [] (let [s (trim (call "skills-json"))] (if (not= s "") s "[]")))

;; ── 各 CLI 的 main（邏輯全在此，CLI 薄殼只呼叫）──────────────────────
(local M {})

;; skills-json — 掃 <skills>/*/skill.json → 印 OpenAI function-tools JSON 陣列。
(fn M.skills-json []
  (let [dir (skills-dir) parts []]
    (when (exists? dir)
      (let [p (io.popen (.. "ls -1 " (shq dir) " 2>/dev/null"))]
        (each [name (: (or (p:read "*a") "") :gmatch "[^\n]+")]
          (let [d (.. dir "/" name)
                meta (.. d "/skill.json")
                runp (.. d "/run")]
            (when (and (not (: name :match "^_")) (exists? meta) (exists? runp))
              (let [obj (jq "{type:\"function\",function:{name:(.name // $dn),description:(.description // \"\"),parameters:(.parameters // {type:\"object\",properties:{}})}}"
                            (read-file meta) (.. "--arg dn " (shq name)))]
                (if obj (table.insert parts obj)
                    (eprint "[skills-json] " name " 的 skill.json 壞了"))))))
        (p:close)))
    (emit (.. "[" (table.concat parts ",") "]"))))

;; run-skill <name> — stdin=參數 JSON → exec <skills>/<name>/run → stdout 結果（截斷保護）。
(fn M.run-skill []
  (let [name (or (. arg 1) "")
        runp (.. (skills-dir) "/" name "/run")]
    (if (or (= name "") (not (exists? runp)))
        (do (emit (.. "[找不到技能 " name "]")) (os.exit 0))
        (let [args (slurp-stdin)
              (out err code) (run ["timeout" "300" runp] args)
              o (trim out)]
          (var res o)
          (when (not= code 0)
            (let [e (trim err)]
              (set res (.. "[退出碼 " code "] " (if (not= e "") e o)))))
          (emit (if (not= res "") (res:sub 1 8000) "（技能無輸出）"))))))

;; mem-append — stdin=一筆 JSON record → 加 ts → append <memory>/log.ndjson。
(fn M.mem-append []
  (let [raw (slurp-stdin)
        ts (iso-now)
        mdir (memory-dir)
        line (or (jq ". + {ts:$ts}" (if (not= (trim raw) "") raw "{}") (.. "--arg ts " (shq ts)))
                 (jq "{raw:$raw, ts:$ts}" nil (.. "-n --arg raw " (shq raw) " --arg ts " (shq ts))))]
    (os.execute (.. "mkdir -p " (shq mdir)))
    (let [f (io.open (.. mdir "/log.ndjson") "a")]
      (f:write (.. line "\n")) (f:close))))

;; ds-chat — stdin={messages,tools} → 一次 DeepSeek 呼叫 → stdout 回應 message JSON。
(fn M.ds-chat []
  (let [key (env "DEEPSEEK_API_KEY")]
    (when (not key) (die 2 "[ds-chat] 未設 DEEPSEEK_API_KEY"))
    (let [model (env "HERMY_MODEL" "deepseek-chat")
          endpoint (env "HERMY_ENDPOINT" "https://api.deepseek.com/v1/chat/completions")
          req (trim (slurp-stdin))
          body (jq "{model:$model, messages:(.messages // [])} + (if ((.tools // [])|length)>0 then {tools:.tools, tool_choice:\"auto\"} else {} end)"
                   (if (not= req "") req "{}") (.. "--arg model " (shq model)))]
      (when (not body) (die 1 "[ds-chat] 請求 JSON 解析失敗"))
      (let [(out err code)
            (run ["curl" "-sS" "-X" "POST" endpoint
                  "-H" "Content-Type: application/json"
                  "-H" (.. "Authorization: Bearer " key)
                  "--data-binary" "@-" "-w" "\n%{http_code}"]
                 body)]
        (when (not= code 0)
          (die 1 "[ds-chat] 請求失敗：curl " (tostring code) " " (trim err)))
        (let [nl (: out :match ".*()\n")   ; 最後一個換行的位置（curl -w 前綴的 \n）
              http (trim (if nl (out:sub (+ nl 1)) out))
              resp (if nl (out:sub 1 (- nl 1)) "")]
          (if (and (not= http "") (= (http:sub 1 1) "2"))
              (let [msg (jq ".choices[0].message" resp)]
                (if msg (emit msg)
                    (die 1 "[ds-chat] 回應無 message：" (resp:sub 1 500))))
              (die 1 "[ds-chat] HTTP " http ": " (resp:sub 1 500))))))))

;; orchestrate — skills-json → 迴圈{ ds-chat → tool_calls 逐個 run-skill → 回饋 } → 答。
(fn M.orchestrate []
  (let [parts []]
    (each [_ a (ipairs arg)] (table.insert parts a))
    (var task (trim (table.concat parts " ")))
    (when (= task "") (set task (trim (slurp-stdin))))
    (when (= task "")
      (die 2 "用法：hermy <任務...>   或   echo ... | hermy"))
    (let [system (trim (or (read-file (.. root "/prompt/system.md")) ""))
          max (or (tonumber (env "HERMY_MAX_STEPS" "12")) 12)]
      (var tools (skills))
      (var messages (jq "[{role:\"system\",content:$sys},{role:\"user\",content:$task}]"
                        nil (.. "-n --arg sys " (shq system) " --arg task " (shq task))))
      (eprint "[hermy] 載入 " (jq "length" tools) " 個技能")
      (mem (jq "{role:\"user\",content:$task}" nil (.. "-n --arg task " (shq task))))
      (var done false)
      (var i 0)
      (while (and (not done) (< i max))
        (set i (+ i 1))
        (let [payload (jq "{messages:., tools:$t}" messages (.. "--argjson t " (shq tools)))
              msg (trim (call "ds-chat" [] payload))]
          (set messages (jq ". + [$msg]" messages (.. "--argjson msg " (shq msg))))
          (let [n (or (tonumber (jq "(.tool_calls // []) | length" msg)) 0)]
            (if (= n 0)
                (let [content (or (jq ".content // \"\"" msg "-r") "")]
                  (emit content)
                  (mem (jq "{role:\"assistant\",content:$c}" nil (.. "-n --arg c " (shq content))))
                  (set done true))
                (for [k 0 (- n 1)]
                  (let [c (jq (.. ".tool_calls[" k "]") msg)
                        fname (jq ".function.name" c "-r")
                        a0 (jq ".function.arguments // \"{}\"" c "-r")
                        a (if (and a0 (not= a0 "")) a0 "{}")
                        id (jq ".id" c "-r")]
                    (eprint "[hermy] → 技能 " fname " " a)
                    (let [result (trim (call "run-skill" [fname] a))
                          toolmsg (jq "{role:\"tool\",tool_call_id:$id,content:$r}"
                                      nil (.. "-n --arg id " (shq id) " --arg r " (shq result)))]
                      (when (= fname "create_skill") (set tools (skills)))
                      (set messages (jq ". + [$msg]" messages (.. "--argjson msg " (shq toolmsg))))
                      (mem (jq "{role:\"tool\",name:$n,result:$r}"
                               nil (.. "-n --arg n " (shq fname) " --arg r " (shq (result:sub 1 2000)))))))))))
        )
      (when (not done) (eprint "[hermy] 到達步數上限 " (tostring max))))))

M
