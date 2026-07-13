# Round 2, Phase 1: Expert Proposals（具體提案）

> 主題延續：**V0 垂直切片的收斂路徑**。各專家兌現 Round 1 反思末尾的「行動指南」，帶可落地的 code / schema 上桌。
> 註：本輪四位專家發言由內部並行 subagent 獨立產出（原定 gemini cli 因 Google 停用免費 OAuth tier 不可用，改用此法保留「多視角對抗」）。

---

## SSE (Senior Software Engineer)

上一輪我承諾帶兩段可立刻落地的 code。原則沒變：**V0 的唯一驗收標準是「這條硬管道能不能從聰明模型生資產、笨模型在標記位置產出合法編輯、斷網時切 Ollama 仍能跑完」**。不直接服務這條管道的，V0 不做。

### 1. 最小化環境變數隔離：`run_isolated()`
反對 V0 上 bubblewrap（相依、平台耦合、維護成本）。V0 的威脅模型只有一個：**笨模型生成的程式碼跑起來時不該繼承整個 shell 環境**（API key / 怪 PATH / HOME 祕密）。用標準庫一個函式解決——**白名單預設空，明列才透傳**：

```python
# lib/isolation.py — 純標準庫，零相依
_ENV_ALLOWLIST = frozenset({"PATH","LANG","LC_ALL","LC_CTYPE","TMPDIR","AI_CORE_ROOT","TRACE_ID"})

def build_isolated_env(extra=None, allowlist=_ENV_ALLOWLIST):
    env = {k: os.environ[k] for k in allowlist if k in os.environ}
    if extra: env.update(extra)
    return env

def run_isolated(argv, *, cwd, extra_env=None, input_text=None, timeout=30.0):
    return subprocess.run(argv, cwd=cwd, env=build_isolated_env(extra_env),
                          input=input_text, text=True, capture_output=True,
                          timeout=timeout, check=False)
```
要點：`env=` 一旦顯式傳入子進程就不繼承父環境（Python 既有語意，非魔法）→「預設空 + 明列才進」正是 SGA 要的「拒絕為預設」，只是落在 env 層；`cwd` 強制顯式（呼應 ext_05）；`timeout` 擋笨模型死循環（比擋惡意 syscall 更真實的威脅）。**威脅模型對得上 V0 的隔離，一行 `env=` 拿到。** namespace/seccomp 留證書升級時談。

### 2. TRACE_ID 透傳：一個環境變數，不要溯源庫
底線維持：**只接受 `_core.py` 流程 + 一個 `TRACE_ID` 環境變數**，反對分散式追蹤庫、反對 V0 上 sqlite。

```python
# lib/trace.py
def current_trace_id():
    tid = os.environ.get("TRACE_ID") or uuid.uuid4().hex
    os.environ["TRACE_ID"] = tid
    return tid
def child_trace_env(parent_id=None):
    return {"TRACE_ID": parent_id or current_trace_id()}
```
`intercept()` **不碰** trace（守住「純宣告/攔截、零副作用」契約）。串接在呼叫端：`run_isolated(..., extra_env=child_trace_env())` 一行跨進程串起來。落盤只用 **JSONL append** 到 `.state/trace.jsonl`，要查就 grep；撐不住再升 ext_07 sqlite（v0.x 的事）。給 SSA：同一 `TRACE_ID` 貫穿「生資產的 LLM 呼叫」與「笨模型填碼的 subprocess」，可事後串——但拒絕 V0 為此扛追蹤子系統。

### 3. V0 範圍收斂：點名該做與該砍
**必做（硬管道）**：① 具體任務＝在錨點 `# AI_CORE:INSERT_HERE` 插入一個函式；② 聰明模型讀框架生資產（一次性，離線快取進 `.data/`）；③ 笨模型在標記處填碼，跑在 `run_isolated` 裡；④ **語法 guard + retry**（`ast.parse()` 不過帶錯誤重試 N 次）；⑤ **降級**：雲端 429/5xx → 切 localhost:11434 Ollama（**這條堅持 V0 就要，砍了失去靈魂**）。
**明確延後**：SSA 的 hub 資產轉移管理 + ext_07 完整 Trace schema、SGA 的 ext_08 namespace/bwrap、ext_05 socket daemon/併發、ext_07 sqlite/golden_trace/失敗回溯。理由一句：**V0 是停止鍵，不是功能展。先讓血流起來。**

### 4. 對 SGA 證書、AIRE 裁剪的回應
- **AIRE 的 AST 裁剪：有條件同意，是 V0 核心資產**。條件：(a) 只做「刪 body、留簽名+docstring」的保守變換，不碰跨檔符號解析（會拖進相依）；(b) 裁剪結果走同一 `TRACE_ID` 可溯源。`prune_for_context(source, keep_anchor) -> str`。
- **SGA 的證書：有條件同意但降規格**。同意 V0 就**讀取並標註**第九軸（`_core.py` 已有此軸，只是消費），但反對「未認證就拒跑」的攔截閘門——V0 全部意義就是讓未認證笨模型先跑、產出 trace 當日後發證素材。`cert_status(metadata)` 回 `approved`/`unverified`/`deterministic`，**標註不攔截**。

**收束**：兩個檔 `lib/isolation.py`+`lib/trace.py` 不到 60 行純標準庫，配 `prune_for_context`+`cert_status`，把隔離/溯源/裁剪/標註四缺口全堵上，全是**機制與標註、無子系統**。提議下一輪直接在 `try_implement/` 端到端串一次。

---

## SSA (Senior System Architect)：資產傳遞協議（Asset Transfer Protocol, ATP v0）

Round 1 我們吵的是同一件事的四個切面。我把它們縫成**一條協議**：V0 流動的不是字串，是**帶 metadata 的資產（asset）**。把「資產」資料結構定死，四人的訴求全掛上去，不需要四套基礎設施。

### 1. 最小傳遞協議：Asset 單一 schema（append-only envelope）
一個 asset 從 `idea.py`（工廠）→ `hub`（轉移）→ `sfc.py`（消費），**全程同一 JSON，逐站追加欄位、不改寫上游**（溯源本質＝沒人偷改上游）。

```json
{
  "asset_id": "a-7f3c", "trace_id": "t-9b21", "kind": "code_snippet", "schema_version": 0,
  "origin":   {"producer":"idea.py","model":"claude-sonnet","prompt_version":"scan_framework@v1","ts":"..."},
  "payload":  {"lang":"python","body":"...","ast_valid":true},
  "pruning":  {"applied":true,"tokens_before":1840,"tokens_after":210,"strategy":"ast_extract:function_def"},
  "target":   {"file":"routes.py","anchor":"### ROUTE_ANCHOR ###","line":42,"resolved_by":"line_assistant"},
  "certificate": {"status":"unverified","model":"ollama:llama3-8b","test_set":null,"stability":null},
  "trace": [
    {"stage":"produce","tool":"idea.py","ok":true},
    {"stage":"prune","tool":"ast_prune","ok":true},
    {"stage":"locate","tool":"line_assistant","ok":true},
    {"stage":"consume","tool":"sfc.py","ok":false,"reason":"syntax_error"}
  ]
}
```
逐項對應：**`payload`+`pruning`（接 AIRE）**＝裁剪是 asset 生命週期裡的一個 transform，把 `payload.body` 換成裁剪版並記 before/after token，非新資產類別；`ast_valid` + `trace.consume.reason` 就是 ext_09 最簡回饋的上下半場。**`target`（接 SSE）**＝行數助手把結果寫回 `target`，sfc 只讀不自己找位置（定位/插入解耦，預備 ext_03 固化）。`kind` V0 只有 `code_snippet`，不開多型。

### 2. Trace ID 流動：接受 SSE 底線，釘死語意
**接受「一個環境變數」**，且這是 KISS 必然——我要的從不是 OpenTelemetry，是 `AI_CORE_TRACE_ID` 環境變數 + append-only `trace[]` 陣列。兩條規則：① 進程內靠 `subprocess(env=...)` 繼承（搭 SSE 軟隔離便車）；② 資產內 `trace_id` 寫進 asset，每站 append。落盤先寫 **NDJSON**（`.data/traces.ndjson`，一行一 asset），**不另開 sqlite**；SFC 既有 `lib/trace.span` 直接複用。grep/jq 即可查「經過哪些站、哪站失敗」。

### 3. Function Hub 在 V0 的最小角色
**做**：① 資產路由（依 kind+target.file dispatch，V0 只一條：`code_snippet`→`sfc.py`）；② trace 接力（透 `subprocess(env=)` 傳 id 並 append）；③ 預算化（既有 context budget 收斂用在「把 snippet+few-shot 塞進笨模型 prompt 的字元預算」）。**不做**：持久化 DB、多消費者編排/DAG/平行、證書簽發（只**讀** status 不蓋章）。一句話：V0 hub＝**轉運站 + trace 接力棒**，非工作流引擎。

### 4. 對 SSE 軟隔離、SGA 證書的回應
- **SSE 軟隔離：完全同意且白嫖**——它和我的 trace 傳遞是同一機制（都用 `subprocess(env=...)`）。合一成傳一個受控最小 env `{AI_CORE_TRACE_ID, PATH...}`，既隔離又可追，一塊石頭。bwrap V0 不上。
- **SGA 證書：有條件同意，砍成「狀態欄位」而非「准入引擎」**。證書在 V0 是 asset 上的 `certificate` 欄位（不是服務），`status` 只有 `unverified`/`syntax_ok`，`test_set`/`stability` 留 null。`execute_in_isolation(asset, *, env) -> ExecResult` 介面同意現在定義，body 就是 SSE 那行 subprocess。**閘門 V0 只開一道（穩定性閘最弱形式）**：`status==syntax_ok` 才允許寫檔；「必要性閘」V0 不查（V0 前提就是確定性程式碼還辦不到，查它空轉）。

**收束 + 給下一輪三題**（答完 schema 可凍結、V0 開工）：① AIRE，`pruning.strategy` 列舉值要哪些？② SSE，找不到 anchor 時 asset 帶 `target.line=null` 往下走讓 sfc 報錯，還是 hub 攔下？③ SGA，`syntax_ok` 之外 V0 還堅持任何 status 值嗎？

---

## SGA (Security & Governance Architect)

先把上一輪讓步釘死：subprocess(env={}) 軟隔離當 V0 起步——**但讓步附帶不可分割條件：受限執行必須在證書上留痕**。否則 V0 是「跑起來了但沒人知道在什麼信任邊界內執行」，那不是治理是僥倖。

### 1. V0 證書的最簡欄位（寄生第九軸 `nondeterministic` dict，零核心改動）
```json
{"nondeterministic": {
  "v":0, "model":"llama3-8b-instruct", "action":"code_edit",
  "status":"uncertified", "sandbox":"subprocess_env_stripped",
  "test_set":null, "stability":null, "issued_at":"2026-06-21T10:00:00Z"}}
```
沒一欄是裝飾：`model` **不准 null**（不知哪個模型生的碼就不該跑，撤照/歸責主鍵）；`status`＝核心准入旗標（V0 幾乎全 `uncertified`）；`sandbox`＝`none`/`subprocess_env_stripped`（`none` 是危險訊號，預設拒絕）；`test_set`/`stability` 留 null（v1 填）；`issued_at` 為撤照/drift 追溯最低要求。
**核發**：笨模型產出編輯、準備落盤的那刻，由行數助手附加，V0 一律 `uncertified`（沒 test_set 不准蓋 approved）。**撤照（V0 最小集，任一即作廢＝拒絕落盤/執行）**：① model 缺失；② `sandbox==none` 而 action 是 `code_edit`；③ §2 裁剪安全審查不過。`approved` 的簽發/撤照留 v1，但**狀態機從 V0 第一天就存在**。

### 2. AST 裁剪安全護欄（本輪主攻）
ext_10「刪除無關代碼區塊」標為 **V0 頭號風險**：「不相干」誰判？`check_auth()`/`verify_permission()`/`validate_token()` 這種**安全價值高、token 成本低**的函式最易被當「不相干」剪掉，笨模型在「看似沒有鑑權」的脈絡生碼自然生出繞過版本——它沒說謊，只是沒看到。裁剪是減法，減法在安全上默認危險。三層護欄全 `ast` 標準庫可落地：
- **A — 保留清單（永不裁剪）**：`SENSITIVE_NAMES = {auth,permission,perm,verify,validate,check,guard,sanitize,escape,token,secret,credential,sign,encrypt,decrypt}`，名字子串命中或函式體呼叫到命中名者，整段保留。
- **B — 裝飾器/呼叫黑名單**：帶 `@requires_auth`/`@login_required`/`@permission_required` 或體內含敏感呼叫者標 `unprunable`（抓行為非命名習慣，比 A 穩）。
- **C — 裁剪後 diff 安全審查（防 B 漏網）**：裁剪是 `action:"prune"` 要附證書。前後各抽「被呼叫函式名集合」做差集，`prune_safety_check(before, after) -> {ok, dropped_sensitive_calls}`；非空→證書作廢、裁剪結果不得進 prompt、**退回未裁剪版（fail-safe）**。
共同原則：**裁剪對安全代碼 fail-closed**——判不準時保留。AIRE 的壓縮率在「安全碼通常很短」前提下幾乎不受影響。

### 3. `execute_in_isolation` 最小介面（為 Level 2 留擴充位）
```python
def execute_in_isolation(cmd, *, constraints=None, stdin_data="") -> dict:
    # V0 body(Level 1 軟隔離): subprocess.run(cmd, env={}, capture_output=True,
    #   text=True, timeout=constraints.get("timeout_sec",5), input=stdin_data)
    # 回傳 {exit_code, stdout, stderr, sandbox:"subprocess_env_stripped", timed_out}
```
回傳 `sandbox` 是**唯一權威來源**，行數助手原封抄進證書。沒經此函式的執行路徑證書 `sandbox` 一律 `none`→被 §1 撤照規則②擋下。即**繞過沙箱＝蓋不出合法 sandbox 值＝不准跑**。ext_08 的輸出清洗 V0 最小版只要求**輸出長度上限**。

### 4. 「拒絕為預設」在 V0 的兩道閘門（預設值都是拒絕）
**閘門一·執行准入**：要寫檔/執行的 `code_edit` 預設 `sandbox:none`→拒絕；放行＝確經 `execute_in_isolation` 跑出 `subprocess_env_stripped` 且 model 非空。**閘門二·裁剪准入**：`action:prune` 結果預設不准進 prompt；放行＝`prune_safety_check` ok。注意 **V0 的「放行」≠「認證」**：放行的是「在沙箱裡、護欄下、留證地跑」，但沒 test_set，`status` 永遠停 `uncertified`。
底線一條：**V0 可以小、可以軟、可以全 `uncertified`，但不可以沉默。** 每個 LLM 碰過的環節都要留下「誰、什麼動作、准不准、關在什麼籠子裡」的痕跡。
給各位：SSE 行數助手負責附證書並抄 `sandbox`；AIRE 裁剪須內嵌護欄 A/B/C；SSA 建議證書加 `trace_id` 欄與 `issued_at` 並列（治理與可觀測性同盟）。

---

## AIRE (AI Research Engineer)：可測量最小閉環 + 裁剪安全契約

沒有裁剪，V0 prompt 會長到笨模型注意力渙散，我們會得到**被污染的實驗**——分不清「飛輪不可行」是架構不行還是 prompt 沒給好。

### 1. V0 評測指標：最小可量化的 `TrimTrace`
反對 V0 上 ext_09 全套四 KPI（Crystallization Depth 還沒固化引擎可量、Latency Gain 是本地 GPU 雜訊）。V0 只要一條 trace、四欄位族，全標準庫採集（token 數粗估 `len(s.split())` 級，不需精確 tokenizer），落 `.data/traces/<trace_id>.json`：
```python
TrimTrace = {
  "trace_id":"t-...", "task":"insert_function",
  "compression": {"tokens_before":1840,"tokens_after":412,"ratio":0.224,
                  "method":"ast_skeleton","nodes_kept":7,"nodes_dropped":23},
  "validity":    {"is_valid_python":True,"anchor_filled":True,"syntax_error":None},
  "degradation": {"tier":3,"provider":"ollama","model":"qwen2.5-coder:7b","fallback_triggered":True},
  "safety":      {"protected_nodes_preserved":True,"violations":[]},
}
```
**「區分模型效能 vs 架構支撐」的做法**（本輪最在意的方法論）：固定 task+method **只變 tier**→量模型效能；固定 degradation **只變 method**（raw vs ast_skeleton）→量架構支撐。報告＝一張 pivot（行 tier、列 method、格 `is_valid_python` 成功率）。raw 列全紅、ast_skeleton 列轉綠，飛輪有效性就**被數據證明**，非嘴巴證明。零相依（json 讀目錄 + 字典計數）。

### 2. AST 裁剪演算法：`skeletonize`（保簽名、保錨點鄰域、刪實作）
```python
def skeletonize(source, anchor_marker="# >>> AI_FILL <<<", protected=frozenset()) -> tuple[str, dict]:
    # L0 錨點所在 function/class → 全文保留
    # L1 同 scope 兄弟 + 被 L0 呼叫到的名字 → 簽名+docstring首行，body 換 ...
    # L2 其餘頂層 def/class → 只留簽名，body 換 ...
    # module 級 import/常數 → 全保留；非 L0 docstring → 刪到首行
```
骨幹全 `ast`：parse→行號定位錨點落在哪節點→L0＝錨點+祖先鏈→L1 對 L0 跑 walk 收 Name/Attribute 與頂層名取交集（ext_10 Dependency Tracking 最小版，只追一層名稱字面、不做型別推導）→非 L0 的 body 換 `...`→`ast.unparse`。策略可單元測試（ext_12 AST Comparison：斷言 nodes_kept、L0 body 未動、L2 body 為 `...`）。

### 3. 正面回應 SGA：壓縮率與安全非零和，接受「保護集」護欄
SGA 疑慮成立且 V0 必處理。不躲也不因噎廢食。三層妥協全落在 `skeletonize` 的 `protected` 參數與 trace `safety` 欄：① **保護集是硬約束**：命中 `{check_auth,validate,verify,sanitize,authorize,assert_*}` 的 def/call **強制升 L0 全文保留**，被裁區若有則以 `# [PROTECTED-KEPT] check_auth(...)` 顯式回填簽名讓笨模型看到「這裡有安全閘門別繞」；② **裁掉即違規即記錄**：`safety.protected_nodes_preserved=False`、名字進 `violations`，**該 trace 在 benchmark 直接判失敗**無論語法是否合法（**安全凌駕語法有效性**，給 SGA 最強承諾：高壓縮但裁掉安全節點＝負分）；③ **護欄策略是 SGA 領地、介面我給**：`protected` 集合怎麼定/能否從 hub metadata `security:true` 自動收集留 SGA 定，我保證 `skeletonize` 當不可逾越約束執行並可測量寫進 trace。trade-off（壓縮率天花板被護欄壓低）我接受且可量（nodes_kept 上升）。

### 4. 對 SSE / SSA 的回應
- **SSE 行數助手 + 插入任務 + 斷網切 Ollama：完全同意無條件**。插入函式的成功可**確定性驗證**（插入後 ast.parse 過、目標節點存在、簽名符合）。行數助手必須純 Python——它正是「LLM 退縮、確定性程式碼接管」的第一個實例，連 `nondeterministic` 軸都不該沾。唯一附加條件：行數助手輸出的錨點位置＝`skeletonize` 的 `anchor_marker` 來源，兩者對齊同一行號座標系否則裁剪錯位。斷網切 Ollama 直接對應 `degradation.tier=3`。
- **SSA 臨時資產 + Trace 含裁剪前後 token：有條件同意**。`compression.tokens_before/after` 已在 trace。但裁剪產物是 **ephemeral 資產**，與固化引擎的耐久資產要在 hub 生命週期區隔（否則 `.data/` 被裁剪殘骸塞爆、污染 ext_09 回放集），trace 加 `"lifespan":"ephemeral"` 標記（屬 SSA hub 治理範疇，此處認領）。

**一句話**：V0 不需要大，需要**能說話**。一條 `TrimTrace`、一個 `skeletonize`、一張 tier×method pivot，就能把「飛輪行不行」從信仰變成可被數據反駁的命題。
