## 4. 設計張力與它的解藥

**張力**：設計哲學要 KISS（前提的必然），但北極星（飛輪 / 自我改進）天然往「大」拉。兩股力反向。

**解藥 = 目標問題**：框架沒有目標問題時會**無限長**——每條軸、每個複合規範看起來都「可能有用」。一旦釘死一個具體目標，它變成**剎車**：只有這個目標用得到的部分才做。**目標問題 = 停止鍵。**

**A / B 兩層分法**（避免把兩個東西當一個做，那正是「感覺在爆炸」的根因）：
- **A：能用的最小工具**（SFC + 幾個組合原語 + singleton 管理）。今天就能交付、忠於初心、天然小。**這是產品。**
- **B：北極星願景**（規範家族 + 治理原則 + 飛輪）。**這是羅盤**，用來指引 A 怎麼長，不是 v1。

**方法論翻轉**（重要）：
> 不要「先把八軸 / 複合 / 多函數合作的標準全定完，再選問題」。
> 要「**先選目標問題，讓它告訴你哪些標準該先定、哪些可以晚點**」。
> 理由：`sub_projs/ver_1/try_implement/DECISIONS.md` 裡那堆懸案的**優先序取決於目標是什麼**。沒有目標只能憑空猜；有了目標，它們自動排好序。

---

## 5. 第一目標問題：程式碼輔助助手

**場景**：手上有一個程式框架。理想做法是用聰明大模型 + AI agent（如 Claude Code）讀框架原始碼 → 生 skill → 輔助編寫相關程式碼。效果好，但很花錢（每次都燒聰明模型）。

**ai_core 的版本**：把它拆成兩層，聰明模型只跑一次性的上層，笨模型跑高頻的下層。

### 上層：讀框架 → 生資產（＝結構化抽取）
- 聰明模型讀原始碼，抽出**有 schema 的資產**：API 簽名 / 用法 snippet / 觸發條件 / few-shot / 護欄 / verifier hook。
- 本質是結構化抽取問題：schema 可驗證，retry_until_valid / guard 咬得住。
- 「結構化抽取也不錯」這個念頭就安放在這——**它不是另一個目標，是同一個目標的上層。**

### 下層：在精準標記的位置填碼（＝受約束生成，§3.2 的「洞」）
- 笨模型最不該做的是「重寫整個檔」（自由度爆炸 → 必錯）。
- **把輸出收縮到窄面**：固定 snippet 寫進 prompt + **行數助手**精準標記「在第 N 行插入 / 填這個洞」。自由度愈小，笨模型愈不會搞砸。
- 行數助手 = 那個確定性函式；模型只負責粗粒度決定「填什麼、填哪」。
- 佐證：Claude Code 自己的 Edit 工具就要求 exact string match、不準重寫整檔——聰明模型都需要這層約束，笨模型只會更需要。

> **正名**：我們生的不是「skill」（一個文字檔），而是 §3.2 的**「帶 LLM 留白的程序」**——可能是一整條調用鏈、隨模型型號變化、且每個 LLM 留白都該帶 §3.4 的證書。

---

## 6. 最小垂直切片（v0）

先別鋪整套規範，先切一條最小的線證明飛輪能轉：

> 拿**一個**真實框架 → 用**一次**聰明模型呼叫，生出一小份資產（程序 + snippet 庫）→ 再讓一個**便宜 / 笨模型** + 行數助手 + retry/guard，產出**「raw 笨模型做不到、包了鷹架就做得到」**的正確程式碼編輯。

**驗收標準**：同一個編碼任務，對照組（raw 笨模型直接寫）失敗 / 出錯，實驗組（ai_core 鷹架）成功。**一條線打通，勝過十條規範。**

（具體選哪個框架、哪個任務、用什麼笨模型、要不要含「固化」步，待議——見 §8。）

### 6.1 v0 切片收斂（2026-06-21，圓桌三輪定案）

`docs/kb/kb-ext/discussion_logs/round_1~3` 的專家圓桌（SSE/SSA/SGA/AIRE 四角色）把上面的抽象切片收斂成**可開工的具體規格**，核心是一個凍結的資料結構 **ATP v0（Asset Transfer Protocol）**：

- **任務**：在錨點 `# AI_CORE:INSERT_HERE` 插入一個函式（成功用確定性驗證：插入後 `ast.parse` 過 + 目標節點存在 + 簽名符合）。
- **單一資料結構**：一個 **append-only 的 asset JSON**，從 `idea.py`（工廠）→ `hub`（轉運站 + trace 接力）→ `sfc.py`（消費）逐站追加欄位、**不改寫上游**。四角色的訴求全掛在它身上，不另開子系統。
- **裁剪**：`skeletonize()`（純 `ast`，保簽名/錨點鄰域、刪實作）；`pruning.strategy ∈ {raw, ast_skeleton}`。
- **隔離**：`subprocess(env=白名單)` 軟隔離（V0 不上 bubblewrap）。
- **溯源**：一個 `AI_CORE_TRACE_ID` 環境變數 + asset 內 `trace[]`，落盤先寫 NDJSON（不上 sqlite）。
- **治理**：證書是 asset 上的 `certificate` 欄位（寄生第九軸 dict），**標註不攔截**；`status ∈ {uncertified, syntax_ok, rejected}`，`rejected` 帶 `reason ∈ {guardrail_violation, retry_exhausted}`（安全否決 vs 能力不足，撤照意義不同）。AST 裁剪三層安全護欄 fail-closed。
- **評測**：`TrimTrace` + tier×method pivot 表（raw vs ast_skeleton），把「飛輪行不行」變成可被數據反駁的命題；配對主鍵 `task_id`、每格 N≥30。

> 全部純標準庫（`ast`/`json`/`subprocess`/`uuid`/`os`），不破壞 `dependencies=[]`。完整規格見 `docs/kb/kb-ext/discussion_logs/round_2_synthesis.md`（統一 schema）與 `round_3_ratification.md`（三條凍結補丁 P1/P2/P3）。**開工次序**：`asset.py → trace.py → skeleton.py → isolation.py → line_assistant.py → demos/v0_pipeline.py`。

> ⚠️ 此切片**尚未開工**——以上為設計收斂，落地時才寫成 `sub_projs/ver_1/try_implement/` code 並更新本節狀態。
