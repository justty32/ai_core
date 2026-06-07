# doc_30 — HTML 展示型儀表板（dark theme dashboard）

- **來源（8 個 HTML 頁）**：
  - `overview.html`（967 行，單頁彙整版「專案現況總覽」）
  - `html/index.html`（首頁 / 導覽中樞）
  - `html/vision.html`（願景演化）
  - `html/architecture.html`（架構元件）
  - `html/status.html`（實作現況）
  - `html/milestones.html`（里程碑 M0–M6）
  - `html/decisions.html`（待決議 / DECISIONS）
  - `html/target.html`（目標問題 v0）
- **狀態**：展示型儀表板（dark theme 網站）。**多為既有內容的呈現版**——把 `roadmap.md` / `core_nature/` / `DECISIONS.md` / `docs/architectures/` 重新編排成網頁。**快照時點 2026-05-25**（頁尾與多處標註）。
- **一行摘要**：一個把 ai_core 構想/規範/原型成果整理成可瀏覽網站的產物；本檔捕捉其「儀表板獨有」內容，其餘交叉引用既有 knowledge_base 檔，不重抄。

> ⚠️ **與現行檔的數據衝突（以現行為準）**：
> 1. **軸數**：儀表板全程稱「**8 軸**」（列 lifecycle/state/interruptible/guarantee/resources/entries/state_dirs/has_entries），並把 `nondeterministic` 當「候選新軸 C1」。**現行事實是 9 軸**（`nondeterministic` 已扶正為第九軸，見 `CLAUDE.md` 與 `doc_05_axes_metadata.md`）。儀表板停在「8 軸 + C1 待議」的較早狀態。
> 2. **D 組決策數**：`decisions.html` 列 **D1–D7（7 項）**，但 `overview.html` 只列 D1–D4（且 D2–D4 內容與 decisions 頁的 D2–D4 不同——overview 把 decisions 頁的 D3/D5 之類重新編號）。兩頁內部不一致；以 `note_06_decisions_and_open_questions.md`（對照 `DECISIONS.md`）為準。
> 3. **測試數**：儀表板稱「try_implement 140 assertions 全綠、3 demo」——與現行原型現況一致（72+68=140）。但儀表板**完全未提正式核心測試**；現行核心測試為 `tests/test_core.py`，pytest 全收集 82 passed（含 65 等）。
> 4. **里程碑**：儀表板稱「M0–M6 皆 0 完成、src/ai_core 只有 protocol helper」。

---

## 一、儀表板作為產物本身（導覽結構）

七頁站點，共用頂部導覽列：**首頁 · 願景演化 · 架構元件 · 實作現況 · 里程碑 · 待決議 · 目標問題**。`overview.html` 是不在導覽列內的「單頁彙整版」，把上述七頁濃縮成一頁長文。

主視覺標語（首頁 + overview 共用，可作專案一句話定位）：
- **「ai_core ＝ 一個讓 LLM 退縮到『意圖翻譯』與『模糊前沿』兩介面的基底；它以『拒絕為預設、憑證准入』治理 LLM 的領地，並用飛輪持續把模糊地帶固化成確定性程式碼。」**
- 副標：**「— 為 Heuristic Learning 提供基礎設施 —」**
- 四徽章：KISS · Lightweight · No wheel-remake · Least dependency；外加 `構想 + 規範 + 原型` / `try_implement: 140 assertions ✅` / `Python 3.11+ · CLI-first`。

---

## 二、儀表板獨有、已捕捉的內容

### 2.1【獨特】ai_core 元件 → Heuristic Learning 角色對照表（首頁專有）

既有 `note_04` 雖記錄了「ai_core 為 HL 提供基礎設施」的定位與 Learning Beyond Gradients 參考，但**這張逐元件對照表是儀表板首頁獨有**：

| ai_core 元件 | 在 HL 中扮演的角色 |
|---|---|
| entry_manager + client | LLM 監督者的呼叫介面 |
| author | agent 產生 / 修改策略函式 |
| hub + funcs/ | 策略函式庫（Heuristic System） |
| author dry-run + retry | 環境回饋驅動的改進迴圈 |
| `--metadata` 介面 | agent 理解工具的唯一入口 |

HL 定義（首頁複述）：coding agent 直接編輯策略程式碼，環境回饋驅動下一輪改進，舊能力固化為測試與版本差異，而非消失在模型權重裡。參考：Learning Beyond Gradients。
交叉引用：`note_04_thinking_layers_model.md` §A（HL 定位來源）、`note_01_vision_and_origin.md`（飛輪/資產工廠論述）。

### 2.2【獨特】現況快照數據面板（status.html + index.html + overview.html）

儀表板把現況量化成徽章/卡片，這些「彙整數據快照」值得保留：

- **首頁狀態快照（2026-05-25）四卡**：
  - 正式規範 = **完整**（`core_nature/` + `docs/architectures/` 完整；8 軸定義、M0–M6 架構文件全篇完成）
  - 探索原型 = **原型**（`try_implement/` 純標準庫；140 assertions 全綠、3 demo 可跑）
  - 正式實作 = **最小**（`src/ai_core/` 只有 protocol helper：`_core.py` + `metadata.py` + `env.py`；M1–M6 未開始）
  - 待決議 = **待處理**（A1–A4、B1–B3、C1–C4、D1–D4，共 **~15 項**）
- **status.html 四個大數字**：`140` try_implement assertions 全綠 · `3` 端到端 demo · `~15` DECISIONS 待決議項 · `0` M0–M6 里程碑已完成。
- **decisions.html 分組計數**：A 組 **4** 項（已原型待扶正）· B 組 **3** 項（開放方向題）· C 組 **4** 項（候選新軸）· D 組 **7** 項（已拍板待追認）。

### 2.3【獨特】status.html 的逐檔現況清單（檔級「完成/最小/待實作/YAGNI」標記）

`status.html` 給每個檔案打了狀態標籤，這是比 `doc_15`/`CLAUDE.md` 更細的**檔級進度盤點**，獨有部分保留如下：

- **`src/ai_core/` 逐檔**：`_core.py`=完成（含註記「A1 懸案：攔截策略與 subcommand 不相容」）、`__init__.py`=完成（公開 register 入口）、`protocol/metadata.py`=完成（fetch_metadata / MetadataView / make_json_error / should_use_json_errors）、`protocol/env.py`=完成（AI_CORE_TOKEN / AI_CORE_FUNCS_DIR…）。
- **尚待實作子模組（標目標里程碑）**：`entry_manager/`（server/queue/ratelimit/backends/cli）=**M1**；`client/`（ai-core-call cli）=**M1**；`hub/`（scanner/exports/server/cli）=**M3**；`small_funcs/`（registry/funcs/cli）=**M4**；`author/`（generator/dryrun/cli）=**M5**；`protocol/server.py`=**YAGNI**（等第二個 manager 出現才從 entry_manager 抽出）。
- **`docs/architectures/` 八檔對照**（01_overview ~ 08_changelog，全部「完成」）——交叉引用 `doc_10`~`doc_17`。
- **`core_nature/` 八檔對照**（overview/lib_spec/cli_spec/data_format/execution_forms/axis_spec/composite_spec/terminal_model，全部「完成」）——交叉引用 `doc_01`~`doc_08`。
- **`try_implement/` 清單**：tools/ **7 個工具** + lib/ **11 個模組** + demos/ **3 個** + docs/ **2 份** + funcs/ 範例。逐檔職責與 `code_02`~`code_04` 一致——交叉引用之，不重抄。

> 註：status.html 把 lib/ 數成「11 個模組」（state_dirs/recovery/memoize/server/singleton/trace/call/llm_call/compose/interact/compose_meta），tools/ 數成「7 個工具」+ meta_core。與 `code_02`/`code_03` 一致。

### 2.4【小幅獨特】overview.html 頁尾一句話索引

`overview.html` 頁尾把專案地圖濃縮成一行（可作快速索引）：
**「規範在 `core_nature/` · 原型在 `try_implement/` · 架構在 `docs/architectures/` · 北極星在 `roadmap.md`」**（2026-05-25 現況快照）。

---

## 三、純呈現版頁面（僅交叉引用，不重抄）

以下各頁經逐字比對，內容＝既有 markdown 的網頁呈現版，無新增獨特資訊（除上節已捕捉者）。對照如下：

| 儀表板頁 | ＝何內容的呈現版 | 交叉引用 knowledge_base |
|---|---|---|
| `vision.html` 願景演化 | roadmap §1 願景／演化鏈／兩個 regime／北極星兩介面／治理（拒絕為預設+憑證准入）／固化引擎手動 vs 自動／設計哲學四原則 | `note_01_vision_and_origin.md` |
| `architecture.html` 架構元件 | docs/architectures 01–02／lib_spec：五元件設計、Singleton Resource Manager Pattern（Entry-managing vs Simple 兩亞型）、`--metadata` 協議規則、八軸表、技術選型表（Python3.11+/uv/litellm/fastapi+uvicorn/platformdirs/pyproject scripts/JSON） | `doc_10`~`doc_14`、`doc_05`（軸）、`doc_11`（協議）、`doc_06`（lib 契約） |
| `status.html` 實作現況 | 各目錄盤點（獨有的檔級狀態標記已於 §2.3 捕捉） | `doc_15`、`code_02`~`code_04`、`doc_01`~`doc_08`、`doc_10`~`doc_17` |
| `milestones.html` 里程碑 | docs/architectures 06 的 M0–M6（＋M3.5）逐項 checklist、依賴關係（主路徑 M0→M1→M2→M3→M3.5→M4→M5→M6；M4 可在 M2 後並行；M5 依賴 M1）、「先解 A1 阻塞級再開 M0」的告警 | `doc_15_arch_project_milestones.md`、`note_06`（A1） |
| `decisions.html` 待決議 | DECISIONS.md 的 A1–A4 / B1–B3 / C1–C4 / D1–D7，逐條「現況/建議/你要決定什麼」、建議處理順序（①A1+A2+A3 →②C1+C2 →③A4 →④B 系列） | `note_06_decisions_and_open_questions.md` |
| `target.html` 目標問題 | roadmap §6/§7/§8：v0 切片、A/B 兩層分法、程式碼輔助助手（上層生資產／下層填碼）、驗收標準、v0 會逼出的懸案表（B1/B3/C2/C1/A4）、開放問題、v0 在 HL 飛輪的位置 | `note_01`（v0/目標問題/開放問題）、`note_06`（懸案） |
| `index.html` 首頁 | 導覽中樞（獨有的 HL 對照表＋狀態快照已於 §2.1/§2.2 捕捉） | — |
| `overview.html` 總覽 | 上述七頁的單頁濃縮（獨有頁尾索引已於 §2.4 捕捉） | 同上各檔 |

---

## 四、結論

此儀表板是 ai_core **2026-05-25 時點**的對外展示快照，內容主體為既有規範/構想/原型的呈現版，**無實質遺漏的新設計決策**。唯一需要無損保留的儀表板獨有產物為：(1) ai_core↔HL 元件角色對照表；(2) 量化的現況快照數據面板；(3) status.html 的檔級進度標記（含各子模組對應里程碑）；(4) overview 頁尾一行索引。閱讀時須留意儀表板停在「8 軸」的較早狀態，現行已為 9 軸。
