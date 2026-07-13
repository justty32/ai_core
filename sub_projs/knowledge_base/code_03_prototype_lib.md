# code_03 — 原型 library（try_implement/lib/）

> **來源**：`try_implement/lib/`（state_dirs / recovery / memoize / server / singleton / trace / call / llm_call / compose / interact / compose_meta / `__init__.py`）
> **狀態**：原型／探索（多為提案，非定案）。全部是 try_implement 的提案／遊樂場實作，**非正式 library**；正式版扶正進 `src/` 前由使用者定奪。例外脈絡同 code_02。
> **一行摘要**：給工具/函式 import 的純標準庫小 library——複合規範參考實作（state_dirs/recovery/memoize）、基礎設施（server/singleton/trace/call）、LLM 包裝（llm_call）、以及八/九軸之外的組合維度（compose/interact/compose_meta）。

相關交叉引用：使用這些 lib 的工具見 [code_02_prototype_tools.md](code_02_prototype_tools.md)；煙霧測試與 demo 見 [code_04_prototype_demos_tests.md](code_04_prototype_demos_tests.md)；組合維度概念見 [doc_21_composition_dimension.md](doc_21_composition_dimension.md)；馴化框架見 [doc_20_taming_framework.md](doc_20_taming_framework.md)；軸/metadata 規範見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)；library 契約見 [doc_06_lib_contract.md](doc_06_lib_contract.md)。

這些不是 CLI 工具，而是給工具/函式 `import` 的小型 library，純標準庫、零外部相依。

---

## 群組一：複合規範家族的參考實作

### `state_dirs.py` — 標準狀態目錄慣例

對應 composite_spec「標準狀態目錄慣例（Standard State Directory Convention）」。當程式宣告 `state: "stateful_external"` 且在 terminal 環境執行時，外部狀態存放在工作目錄下四個位置之一：
- `.config/<name>/` 或 `.config/<name>.json` — 程式不修改的設定。
- `.cache/<name>/` 或 `.cache/<name>.json` — 可隨時刪除。
- `.state/<name>/` 或 `.state/<name>.json` — 程式目前所在 stage，可重置。
- `.data/<name>/` 或 `.data/<name>.json` — 累積成果，需備份。

`class StateDirs(program_name, base=None)`：
- `dir_path(kind)` / `file_path(kind)` — 資料夾形式 `.<kind>/<name>/` 與單檔形式 `.<kind>/<name>.json`。`KINDS = ("config","cache","state","data")`，與 `_core.py` 的 `_STATE_DIR_VALUES` 對齊。
- `ensure_dir(kind)`（lazy mkdir）、`child(kind, *parts)`（子檔路徑）。
- `read_json(kind, default)` / `write_json(kind, obj)` — 單檔 JSON 往返（慣例：單檔必須 .json 且內容為 JSON object）。
- `declared(*kinds)` — 產可餵給 `register(state_dirs=...)` 的片段，如 `{"state":"stateful_external","state_dirs":["state","data"]}`。

**設計決策**：base 預設為 cwd（`Path(".")`），是「工作目錄下」而非 `$HOME/.config`（刻意與 XDG 不同，讓副作用範圍可預期落在當前目錄）；四目錄不強制全存在；lazy 建立。

### `recovery.py` — 中斷恢復慣例

對應 composite_spec「中斷恢復慣例（Interruption Recovery Convention）」。當程式宣告 `interruptible ∈ {resumable, rollback, resettable}` 或 `guarantee: "transactional"`，恢復能力統一落地為：
- `.state/<name>/recovery.json` — 恢復記錄 manifest（程式間真正的合約）。
- `.state/<name>/recovery/` — opaque payload（進度資料/journal，選填）。

manifest 最小欄位：`status`（`in_progress`｜`done`）、`mode`（`resume`｜`rollback`｜`reset`）、`ts`（ISO 8601）、`payload`（選填相對路徑）。

三種恢復模式：`resume`（從斷點接續）、`rollback`（撤銷未提交修改）、`reset`（重置到安全點）。

`class RecoveryManager(program_name, base=None, mode="resume")`（內部用 `StateDirs`）：
- `manifest_path` / `payload_dir` / `payload_path(*parts)`。
- `read()`、`begin(payload, **extra)`（status=in_progress）、`checkpoint(...)`（刷新 ts/payload）、`complete(delete=False)`（status=done 或刪記錄）。
- `detect()` → `(action, manifest)`：記錄不存在或已 done → `("clean", None)`；in_progress → 依記錄裡的 mode。
- `startup(on_resume, on_rollback, on_reset)` — composite_spec 的「啟動恢復流程（標準演算法）」：偵測自動（不需 flag），依 action 呼叫對應 handler。
- `session(payload, **extra)` context manager — 進入 begin(in_progress)、正常離開 complete(done)；途中拋例外或被 kill 則停在 in_progress，下輪 startup() 可偵測。**把「崩潰留下半完成記錄」變成預設行為。**

**設計決策**：偵測 auto（不需 flag，合規範）；不新增 metadata 欄位（恢復能力由既有軸宣告，本 library 只承擔 on-disk 合約）。

### `memoize.py` — 記憶化慣例（**尚未定案**）

⚠️ 此慣例在 core_nature 中**尚未定案**（session_resume 列為「狀態恢復家族第二條」，留三個開放決策）。使用者外出期間授權自行拍板，本檔是 try_implement 提案，**未動 `src/ai_core`**。

用 `.cache/<name>/<input-hash>` 做「輸入→輸出」快取。目的是**效能**（避免重算），非正確性——與 idempotent 的區別：idempotent = 正確性（重跑不累積副作用），memoized = 效能（重跑直接取快取）。

`class Memoizer(program_name, base=None, version="1", ttl=None)`（內部用 `StateDirs`）：
- `key(stdin, args, files, extra)` — `sha256(canonical_json({v, stdin, args, file_digests, extra}))`。`file_digest(path)` 大檔只存 digest 不揉全文。
- `get(key)` / `put(key, output)` / `invalidate(key)` / `clear()`。get 支援 ttl 過期即清。
- `cached_call(compute, stdin, args, files)` → `(output, hit)`：命中回快取，否則跑 compute 存起來。
- `declared()` — 回**提案中**的 memoized metadata 形狀（如 `{"memoized":{"version":...}, "state":"stateful_external", "state_dirs":["cache"]}`）。

**三個開放決策（自行拍板，供使用者評估）**：
1. **cache key 輸入組成**：預設納入 stdin + args，可選納入檔內容 digest；由呼叫方明確指定，不臆測。
2. **失效策略**：預設靠 .cache「可隨時刪」語意（無 TTL，KISS）；提供兩個 opt-in：`version`（bump 即整批失效）、`ttl`（逐筆過期）。
3. **metadata 欄位（關鍵差異）**：memoized **沒有對應軸值可隱含**（guarantee enum 是 {none,idempotent,transactional}，塞不進 memoized）。建議很可能需要一個新頂層欄位，但這會動到軸層（使用者禁區），故只示範形狀、不塞進真 `register()`（真 register 目前會以 unknown field 拒收）。這正是給使用者「memoized 純屬 runtime vs 入軸」決策的具體素材。

---

## 群組二：基礎設施

### `server.py` — persistent server 標準 lifecycle

對應 thinking_pending §3「持久性程式建議設計成 server」。把 SFC forge 的「啟動→就緒→逐行處理→關閉」迴圈泛化成可重用 library；SFC forge 是此模式的具體實例。

`class NDJSONServer(name, route_key="cmd")`：
- 標準 lifecycle：`start`（on_start 載入資源）→ `ready`（往 stderr 印一行 ready 訊號可被探測）→ `serve`（逐行讀 stdin JSON request、dispatch、回一行 JSON response）→ `shutdown`（收到 shutdown 指令或 EOF，跑 on_shutdown）。
- `handler(name)` 裝飾器 / `register(name, fn)` / `on_start(fn)` / `on_shutdown(fn)`。
- handler 簽名 `fn(req: dict) -> dict`；回傳包進 `{"ok": true, **result}`，拋例外則 `{"ok": false, "error": "..."}`。
- 內建指令：`ping`（→pong）、`list`（列出 handler）、`shutdown`。
- `serve(stdin, stdout, stderr)` 接受任意 readable/writable（預設 sys.std*），方便用 `io.StringIO` 測試。
- **`serve_socket(socket_path)`（2026-06-08 新增，解 Gap G）**：與 `serve()` 同 lifecycle，但走 **Unix domain socket** 傳輸而非 stdin/stdout。關鍵差別：**多個獨立的 one-shot caller 可連上同一個 server 實例**，共用同一份 handler 閉包狀態（如 entry manager 的 `RateMeter`）→ consume rate 能**跨呼叫累計**——這正是 stdin/stdout 做不到、Gap G 要解的點。語意仍是 singleton（serial accept、一次處理一個連線、不開 thread）；單一連線內可送多行請求，連線 EOF 只結束該連線，唯有 `shutdown` 指令停整個 server 並清掉 socket 檔（啟動時也清 stale socket）。`serve()` 與 `serve_socket()` 共用 `_process_line()`（抽出的單行處理）。

**設計決策**：預設介面用 stdin/stdout NDJSON（最 KISS，免 port/http）；`serve_socket` 提供長駐單例所需的「多 caller 連同一個」傳輸（選 Unix socket 而非 HTTP：純標準庫、POSIX 原生、免 port）。剩餘規範議題「persistent singleton 如何被多 one-shot caller 共用（連線/排隊/逾時語意）」是 DECISIONS E2，v0 真接小模型時正式化（見 [note_06_decisions_and_open_questions.md](note_06_decisions_and_open_questions.md)）。

### `singleton.py` — singleton 資源（queue + consume rate）

對應 thinking_pending §4。LLM Entry Manager 是具體實例：資源一次只能處理一個請求，請求者是多個互不認識的 caller → queue + consume rate（token/錢/算力多維度）。抽成三塊：

- `class RateMeter(limits=None)` — 多維度用量累計器（dict 累計任意命名維度，不寫死只有 token）。`record(**amounts)`、`total(dim)`、`usage()`、`remaining(dim)`、`exceeded()`（回已超限維度列表）、`would_exceed(**amounts)`（若再記會超限的維度）。limits 選填，**超限只回報不強制 abort**（policy 與 mechanism 分離）。
- `class RequestQueue` — FIFO，`enqueue(payload)`→ticket id、`dequeue()`（跳過已取消）、`cancel(tid)`（用 ticket id 取消）、`pending()`。對應 §4「enqueue/dequeue/cancel」標準介面。
- `class SingletonResource(name, limits=None)` — 綁起來：`submit`/`cancel`、`serve_one(worker, cost_fn)`（取一個跑、記 consume）、`drain(worker, cost_fn, stop_on_limit=True)`（一次處理一個直到 queue 空；超限即停）。

**設計決策**：queue 純記憶體單行（singleton 一次一個，無需多佇列）；多個不同 LLM entry 各自消費由「多個 SingletonResource 實例」表達。

### `trace.py` — 調用鏈追蹤（輕量方案）

對應 thinking_pending §5「調用鏈設計」，採該節的「輕量方案」：每工具在 stderr 輸出結構化 JSON log，成本接近零，由最外層 caller 決定是否收集。調用鏈在 shell 世界 = process 與其子 process / pipeline 前後節點。

- **trace id 傳遞**：兩個環境變數——`AI_CORE_TRACE`（整條鏈共享的 trace id）、`AI_CORE_SPAN`（呼叫方 span id，本工具 span 以它為 parent）。`current_trace()`（無則自生 = 鏈頭）、`parent_span()`、`child_env(base=None)`（給 subprocess 的環境：帶 trace id + 以目前 span 為 parent）。
- **span 事件** `span(name, stderr=None, **fields)` context manager：印 start（含自身 span id），離開印 end（含 duration）；區塊內把自身設為 `AI_CORE_SPAN`，spawn 的子 process 以它為 parent，離開還原。
- **`class Collector`**：`add_line` / `add_text` 吃 stderr 文字（非 JSON 行自動略過），`tree()` 以 span/parent 重建調用樹（容忍亂序與缺漏，缺 end 的標 incomplete），`render()` 畫成縮排文字（incomplete 加「⚠ incomplete」標記）。

**已實際接進工具**：router 與 sfc forge 的 dispatch 都 trace-aware（見 code_02）。

### `call.py` — 跨邊界統一呼叫

對應 thinking_pending §6 與 thinking_oop。核心立場：HTTP 調用、shell 調用、程式內函數調用本質是同一件事（函式呼叫跨越不同邊界，只是傳遞機制不同）。因 ai_core I/O 都是文字，三種邊界統一收斂成 `target.call(text_in: str) -> text_out: str`：

- `class InProcess(fn)` — 同 process 直接函式呼叫。
- `class Subprocess(cmd, check=True)` — 開子 process，text 餵 stdin、收 stdout；**自動帶 `trace.child_env()`**（子鏈接上同一條 trace）。
- `class Http(url, method="POST", timeout=30, headers=None)` — POST text 當 body、收 response body；用標準庫 `urllib`，不引入 requests（least dependency）。
- `from_spec(spec, registry=None)` — 由 JSON 設定建 target（kind ∈ subprocess/http/in_process）；呼應「router 不在意 mapping 目標是檔案還是資料庫」，這裡再擴到網路。
- `call(target, text)`（鴨子型別，有 `.call` 即可）、`call_json(target, obj)`（兩端 JSON 編解碼傳結構化資料）。

**待設計（誠實標記）**：thinking_pending §6 指出跨網路邊界的**分散式狀態**（一致性、失敗恢復、冪等）與本地調用有本質差異，需專門設計。本 library 只做「呼叫傳遞」，不處理分散式狀態——retry/冪等搭配 `lib/memoize`、軸值、或上層 `compose.retry_until_valid`。

### `llm_call.py` — llm_call 基底 + context packing（CLAUDE.md 元件 2）

把 LLM 視為一個（隨機的）函式 `llm_call(str) -> str`，再疊 context 與 post-processing 形成更具語意的新函式。

- **基底** `llm_call(prompt, *, backend=None, **opts)` — 把 prompt 丟給 backend 回文字；opts（temperature/seed/max_tokens）原樣轉給 backend。
- **打包** `bind(*, system, prefix, suffix, postprocess, backend, **opts) -> f(str)->str` — 把 system/prefix/suffix/postprocess 疊上去產出新的 str→str（無狀態 one-shot，可直接餵給 `compose`）。對應 CLAUDE.md 範例：
  ```python
  coding_q = bind(system="you are a professor of coding...",
                  postprocess=lambda o: o + " -- at 20240505")
  coding_q("how to sort a list?")
  ```
- **可插拔 backend**：`class Backend`（介面 `complete(prompt, **opts)->str`）；`EchoBackend`（預設，回 `echo: {prompt}`）；`ScriptedBackend(responses)`（依序循環吐出，元素可為字串或 `fn(prompt)->str`，**用來在測試模擬「同輸入不同輸出」的隨機性**）；`FnBackend(fn)`。`set_default_backend()` 切換全域預設。
- **真 backend（2026-06-08 實作）**：上層 bind/compose 完全不變，新增兩個接真 API 的 Backend，都走 `lib/call.Http`（純 urllib、零外部相依）：
  - **`OpenAIBackend(base_url, model, api_key=None, ...)`** — OpenAI 相容 `/chat/completions`，**吃本地小模型**（ollama → `http://localhost:11434/v1`、llama.cpp → `:8080/v1`、vLLM）與相容代理（OpenRouter）。這正是 roadmap「假設只剩便宜本地小模型」的目標 backend。prompt 整段當單一 user message（`bind()` 已把 system/prefix/suffix 疊進 prompt）。`temperature/top_p/stop/seed/max_tokens` 透傳。
  - **`AnthropicBackend(base_url, model, api_key=None, version="2023-06-01", ...)`** — Anthropic Messages API `/v1/messages`，回應 content block 陣列串接所有 text block。
  - 兩者都用**延遲 import `lib/call`**（只用 EchoBackend 時不付 trace 相依代價）。
- **`backend_from_env(env=None)`** — 依環境變數挑 backend（元件 1「統一 LLM 呼叫入口」把 provider 選擇集中在環境，工具/bind 程式碼不必改）：`AI_CORE_LLM_PROVIDER`（openai|anthropic|echo，預設 echo）/ `_BASE_URL` / `_MODEL` / `_API_KEY` / `_MAX_TOKENS`。**未設定 / 未知 provider 則回 `EchoBackend`**（離線/測試友善）。entry manager 與 idea 工具都用它決定打哪個真 LLM。E1 已知小缺口：`OpenAIBackend` 送 `max_tokens`，對本地模型正確、但 OpenAI 官方新模型（o1/o3）要 `max_completion_tokens`（見 note_06 E 區）。

**設計立場**：llm_call 是系統裡唯一的非確定性函式；馴化它的手段不寫在這裡，而在 `lib/compose`（同一套組合任意函式的組合子）。詳見 [doc_20_taming_framework.md](doc_20_taming_framework.md)。真 backend 的首個真實應用 = 點子捕捉軌 dogfood，見 [doc_22_workflow_and_idea_track.md](doc_22_workflow_and_idea_track.md)。

---

## 群組三：八/九軸之外的組合維度

### `compose.py` — 組合子（組合維度）

主張「函式怎麼一起動」是與八軸正交的維度。每個組合子吃若干 `f(str)->str` 回傳新的 `f(str)->str`（型別不變故可無限疊套）。

- **順序合作**：`pipe(*fns)` — 管線（= 調用鏈的純函式版，`lib/trace` 負責可觀測性）。
- **並聯合作**：`fanout(*fns)`（同輸入多分支回 list）、`fanout_reduce(fns, reducer)`（map→reduce 收成單一輸出）。
- **條件合作**：`route(selector, table, default)`（依條件分派，= Switch 的組合子化）、`with_fallback(primary, fallback, is_ok)`（主拋例外或輸出不合格則走備援）。
- **分治合作**：`decompose(split, sub, join)`（拆→各自處理→合；對 LLM 特別有用，大任務拆小更易驗證）。
- **馴化隨機性**（重點，對任何「吵的函式」都適用，LLM 只是最典型那個）：
  - `retry_until_valid(fn, validate, retries=3, on_exhausted=None)` — 拒絕採樣：重抽到輸出通過 validate，用盡則回 on_exhausted 或拋 `ValidationError`。
  - `vote(fn, n=5, key=None)` — 自一致（self-consistency）：同輸入抽 n 次取眾數（key 正規化後比對，回第一個命中的原始輸出）。
  - `best_of(fn, n, score)` — 抽 n 次取 score 最高者。
  - `guard(fn, validate, repair)` — 驗證→修復：壞輸出交給 repair（可為另一次 LLM 呼叫）修。

### `interact.py` — 多函數交互（組合維度的「來回」版）

對應組合維度文件 §5。`compose` 處理**單向組合**（資料從輸入流到輸出）；`interact` 處理更難的**交互**：函式之間來回，需要 compose 沒有的兩樣東西——**共享狀態**與**終止條件**。

- **黑板（blackboard）driver** `run(participants, state, until, max_rounds=8)`：每輪讓每個 participant 依序讀寫共享 `state`（dict），直到 `until(state)` 成立或達上限。回 state 含 `_rounds`、`_stopped`（`until`｜`max_rounds`）。
- **`actor_critic(task, actor, critic, max_rounds=3)`**：actor 生成草稿、critic 批評（LLM-as-judge），反覆修到驗收或用盡。回 `{draft, accepted, rounds, history}`。把驗證從一次性 pass/fail 升級成「不過就帶理由重修」。
- **`debate(task, debaters, judge, rounds=1)`**：多方各出 argument，judge 收斂成裁決；rounds>1 時把上一輪裁決回灌。回 `{verdict, rounds, arguments}`。

**為什麼 `max_rounds` 是強制的**：LLM 交互最容易出事的地方是**不會停**（actor 與 critic 無限互踢），故 `max_rounds` 設成必有的安全閥（預設給值），即使 `until` 永不成立也保證收斂。`compose`（單向）與 `interact`（來回）在「actor-critic」處交會。

### `compose_meta.py` — 組合的軸推導規則（候選新概念）

對應組合維度文件 §3。`compose` 只組合**行為**；本模組多做一件事：從成員函式的八軸 metadata **推導出複合函式的 metadata**。若成立，hub 就能對「臨時組起來的複合函式」自動算 metadata，不必人工標註——是把組合維度接回八軸的橋。

- **核心物件** `@dataclass MetaFn(fn, meta, name)` — callable + 八軸 metadata，`__call__` 即呼叫 fn。
- **推導規則**：
  - **guarantee**（強度序 none < idempotent < transactional）：`pipe` 取**最弱**（一條鏈只要有一段不能安全重試，整條就不能）；`fanout` 若分支共用 state_dir → 並發寫衝突 → 退化為 `none`，否則取最弱。
  - **state**：聯集（任一成員 stateful_external，整體就是）。
  - **state_dirs**：聯集（讓 hub 算出複合函式會碰哪些目錄）。
  - **lifecycle**：複合呼叫本身回 `one_shot`；有 persistent 成員則加 `requires_persistent` 列出相依——**八軸沒有「依賴外部 server」的 lifecycle 值，這個推導正好把缺口暴露出來**。
- **組合子**：`derive_pipe(metafns)` / `mpipe(*metafns)`（meta-aware pipe）、`derive_fanout` / `mfanout_reduce(metafns, reducer)`（含共用 dir 衝突偵測 + `_warning`）、`mretry(metafn, validate, retries=3)`（**強制檢查被包函式至少 idempotent**，否則拒絕組裝——把「retry 前置條件」變可執行檢查）。

**簡化與保留**：guarantee 強度序是簡化（transactional 與 idempotent 嚴格說非全序，此處為能算 min 而定序）；`interruptible` 的推導較微妙（pipe 段間是天然 checkpoint），本原型先不推導，列後續。
