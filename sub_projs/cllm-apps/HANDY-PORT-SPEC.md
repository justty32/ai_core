# handy 工具集多語言移植 SPEC（Python / C++ / Common Lisp）

把 `~/repo/ai_core/sub_projs/handy` 的四個工具（**llme / zhtw / wf / mail**）以 **shell-out 方式**移植到各語言，落在 `~/code/cllm-apps/<lang>-handy/`，並照 cllm-apps 房規**整合 cllm 周邊工具**（anthropic-proxy / llm-login）。

- **原型（真相源，逐檔讀）**：`~/repo/ai_core/sub_projs/handy/llme/_exec.fnl`、`.../zhtw`、`.../wf`、`.../mail`。**行為要 1:1 對齊 Fennel 版**（下列契約是摘要，細節以原始碼為準）。
- **shell-out 原則**：各語言只重寫「dispatcher / 膠水邏輯」，真正的「手」仍轉呼外部命令（`llm` cllm CLI、`claude`、以及**同語言 sibling 工具**）。不用 cllm binding。
- **語言目錄**：`python-handy/`（`#!/usr/bin/env python3`）、`cpp-handy/`（編譯成執行檔，附 `build.sh`）、`lisp-handy/`（`#!/usr/bin/sbcl --script` 或 `scripts/run.sh` + uiop）。

## 通用約定（四工具共通）

- **shell 安全轉義 `shquote(s)`**：`'` → `'\''`，整體單引號包住。組命令一律用它（吃中文/空白/旗標）。
- **exit code 透傳**：轉呼子命令，子命令回幾就回幾。
- **stdin 讀取**：只有 stdin **非 tty**（被 pipe/重導）才讀，避免互動卡等 EOF。偵測法＝`test -t 0`（跑一下、看退出碼；0＝tty）。讀就一次讀到 EOF。
- **script-dir**：解出「本執行檔所在目錄」（含解 symlink），供 (a) 找同層 sibling 工具 (b) 找 `configs/` / `inbox/`。
- **env override 命名一律保留原樣**（見各工具），測試靠它把外部命令換成 `echo`。
- **sibling 解析**：工具互呼（zhtw→llme、wf→llme/mail）**預設解到同語言的同層 sibling**（`<script-dir>/llme` 等），並讓對應 env（`WF_LLME` 等）可覆寫。

---

## 1) llme — 多 endpoint dispatcher

`llme <endpoint> [llm 的其餘參數...]` → 轉呼 `llm --config <configs>/<endpoint>.json [--api-key <k>] <其餘參數...>`。

- **config 目錄**：env `LLME_CONFIG_DIR` ＞ `<script-dir>/configs`。
- **llm 執行檔**：env `LLME_LLM` ＞ `"llm"`。
- **參數檢查**：無 endpoint 或 `--help`/`-h` → 印用法（無 endpoint exit 2、help exit 0）。用法要列出 config 目錄下可用 endpoint（掃 `<configs>/*.json`，**`_` 開頭的排除**）。
- **config 不存在** → 印錯誤＋用法，exit 2。
- **auto-inject api key**：若使用者**沒**自帶 `--api-key`（掃其餘參數有無 `--api-key` 或 `--api-key=…`），依序找 env：① `LLME_KEY_<EP>` ② `<EP>_API_KEY`，找到就在使用者參數**之前**插 `--api-key <值>`。`<EP>`＝endpoint 名**大寫、非英數轉 `_`**（如 `deep-seek`→`DEEP_SEEK`；`deepseek`→`DEEPSEEK`→`DEEPSEEK_API_KEY`）。
- 組命令 exec、透傳 exit code。

## 2) zhtw — 繁中翻譯薄包裝（人格單檔）

固定常數（烤死）：`ENDPOINT="deepseek"`、`SYSTEM="你是專業翻譯引擎。把使用者提供的文字翻成自然、道地的繁體中文。只輸出譯文本身，不要加任何解釋、引號或前後綴。"`、`PREFIX="翻譯以下內容：\n"`、`FLAGS=["--temperature","0.2","--top-p","0.9","--max-tokens","4096"]`。

- 輸入：有位置參數＝所有參數併成一句；否則讀 stdin（trim）。皆空 → 用法 exit 2。
- `prompt = PREFIX + text`。
- 轉呼 sibling `llme`：`llme deepseek <FLAGS...> --system <SYSTEM> -- <prompt>`，透傳 exit。
- sibling 解析：預設 `<script-dir>/llme`；可用 env `ZHTW_LLME` 覆寫（原型寫死 `"llme"`，移植版改為「預設同層 sibling、env 可覆寫」，並在 README 註明此偏差）。

## 3) wf — 兩層任務派發器

三模式：`-b/--brain`（llme）、`-a/--agent`（claude）、`-i/--inbox`（轉 mail send）。前置旗標解析、`--` 停止、`-h/--help`。

- **輸入**：前置旗標後，argv＝任務；stdin（非 tty 才讀，trim）＝上下文 `ctx`。`prompt`＝有 task 時 `task`（有 ctx 再接 `"\n\n--- 以下為透過管線傳入的上下文 ---\n"+ctx`），無 task 時＝ctx。皆空 → 用法 exit 2。
- **路由決策順序**：強制旗標 ＞ **啟發式**（免 LLM）＞ **LLM 分類**。
  - 啟發式 `heuristic-route(prompt)`：對 `prompt.lower()` 做子字串比對，`AGENT-PATS`/`BRAIN-PATS`（**清單見原始碼 wf，一字不差照抄**）。只中 agent→`"agent"`；只中 brain→`"brain"`；都中或都沒中→`nil`（交 LLM）。
  - LLM 分類 `classify(prompt)`：`capture` 執行 `llme <endpoint> --temperature 0 --max-tokens 16 --system <CLASSIFY-SYS> -- <prompt>`，讀 stdout 轉大寫，含 `BRAIN`→brain、含 `AGENT`→agent、否則 `"agent"`（安全預設）。`CLASSIFY-SYS` 見原始碼照抄。
  - 印一行到 stderr：`[wf] 路由：<route>（<how>）`，how＝`強制`/`啟發式・免 LLM`/`llme 分類，endpoint=<ep>`。
- **env**：`WF_ENDPOINT`(deepseek)、`WF_LLME`(sibling llme)、`WF_CLAUDE`(claude)、`WF_PERMISSION`(acceptEdits)、`WF_MODEL`、`WF_EFFORT`、`WF_SYSTEM`(預設 `"與使用者溝通時用繁體中文。"`，設空字串關掉)。
- **執行**：
  - `inbox`：轉 sibling `mail send`（task/ctx 分開傳——有 argv task 且有 ctx 時把 ctx 經 stdin 餵 mail：`printf %s <ctx> | mail send <task>`；否則 `mail send <task-or-ctx>`）。
  - `brain`：`llme <endpoint> --stream [--system <system>] -- <prompt>`。
  - `agent`：`claude -p --permission-mode <WF_PERMISSION> [--append-system-prompt <system>] [--model <WF_MODEL>] [--effort <WF_EFFORT>] -- <prompt>`。
  - 透傳 exit code。

## 4) mail — inbox 協議（非同步交接）

子命令 `send` / `list`(`ls`) / `run`；信箱 `INBOX_DIR` ＞ `<script-dir>/inbox`，歸檔 `<inbox>/done`。

- **send `<任務...>`**：argv＝task、stdin（非 tty 才讀，trim）＝ctx。皆空 exit 2。`real-task`＝有 task 用 task、否則 ctx。`slug`＝slugify(real-task)。寫 `<inbox>/<slug>.md`，撞名補 `-2/-3…`。信件格式：
  ```
  # <slug>

  ## 任務
  <real-task>

  ## 上下文
  <有 argv-task 且有 ctx 時填 ctx，否則「（無）」>

  ---
  （此信由 handy inbox 投遞；請完整完成信中的任務。處理者會於完成後將本信歸檔到 done/。）
  ```
  印 `已投遞：<path>`，exit 0。先 `mkdir -p <done>`。
  - **slugify**：trim → 空白轉 `-` → 去掉 `/\:*?"'<>|` → 連續 `-` 併一個 → 去頭尾 `-` → **UTF-8 安全**取前 16 字元（不切半個字）→ 空則 `"task"`。
- **list**：列 `<inbox>` 頂層 `*.md`（`done/` 不含）的 stem；空則印「（inbox 空：沒有待處理的信）」。
- **run [slug|--dry]**：claude＝`WF_CLAUDE`(claude)、permission＝`WF_PERMISSION`(acceptEdits)。`--dry` 只列不跑。指定 slug 只處理該封，否則全部待處理。每封：讀信 → prompt＝`"以下是一封待處理的任務信，請完整完成信中的任務：\n\n"+letter` → `claude -p --permission-mode <permission> -- <prompt>` → **exit 0 才** `mv` 到 `done/`、印 ✓；非 0 保留、印 ✗ 到 stderr、記 failed。全跑完有 failed 則 exit 1。
- **信件是 runtime 產物、不進版控**：`inbox/.gitignore`（`*.md` + `!.gitkeep`）、`inbox/.gitkeep`、`inbox/done/.gitkeep`。

---

## cllm tools 整合（照 cllm-apps 房規）

參考 `~/code/cllm-apps/janet-try-1/scripts/{up.sh,vendor.sh}` 與 `config/config.example.json`。每個 `<lang>-handy/` 附：

- **`configs/`**：`deepseek.json`（`endpoint=https://api.deepseek.com/v1/chat/completions`、`model=deepseek-chat`、`timeout_ms=120000`，keyless）＋ `anthropic.json`（`endpoint=http://127.0.0.1:8787/v1/chat/completions`、`model=claude-opus-4-8`，指向本機 anthropic-proxy）＋ `_template.json`。**config 只放 Client 欄位、不放註解鍵**（glaze 嚴格）。
- **`scripts/vendor.sh`**：照 janet 版——從 `$CLLM_ROOT/build/tools` 複製 `anthropic-proxy`/`llm-login`/`liblogin.so` 進 `vendor/cllm-tools/`（gitignored）。
- **`scripts/up.sh`**：照 janet 版——`anthropic` 模式起 proxy sidecar（找二進位序：`vendor/` ＞ `$CLLM_ROOT/build/tools` ＞ PATH；TCP health-check）＋備憑證（`ANTHROPIC_API_KEY` ＞ `llm-login token` ＞ config），然後**用本 app 的工具跑**：`exec <script-dir>/../llme anthropic "$@"`（把 proxy endpoint 當一個 llme endpoint 用）。openrouter 模式同 janet。
- **README.md**：架構圖、結構、怎麼跑（離線 dry-run＋真後端 DeepSeek＋經 proxy 打 Anthropic）、與 Fennel 原型的差異、與同語言 sibling 解析說明。

## 測試要求（每語言都要做、寫進 README 或 test）

- **dry-run 轉發**（不觸網）：`LLME_LLM=echo <app>/llme deepseek --stream hi`（應印 `--config …/deepseek.json --stream hi`）；`DEEPSEEK_API_KEY=FAKE LLME_LLM=echo <app>/llme deepseek hi`（應含 `--api-key FAKE`）；`WF_LLME=echo WF_CLAUDE=echo <app>/wf 修改 foo.py`（應 `[wf] 路由：agent（啟發式・免 LLM）`）；同理 brain/`-i`；`mail send`/`list`/`run --dry`/`run`（`WF_CLAUDE=echo`）歸檔 done/。
- **語言特定**：C++ 必須 `./build.sh` 編過（g++ 或 clang++，C++20/23）；CL 必須能用 sbcl 跑起來（uiop 做 subprocess/檔案/env；SBCL 2.6 有 `sb-ext`/`uiop`）。
- **UTF-8**：slug 中文不可切半字；shquote 中文正確。

## 交付與回報

- 全部檔案放 `~/code/cllm-apps/<lang>-handy/`，四工具可執行（`chmod +x`）。
- 回報：建了哪些檔、每工具 dry-run 測試結果（貼關鍵輸出）、編譯/執行是否綠、與 SPEC 的任何偏差。
