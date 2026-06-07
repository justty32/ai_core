# 00_INDEX — ai_core 知識庫總索引

> **這是什麼**：把原本散落在 `roadmap.md` / `core_nature/` / `try_implement/` / `docs/` / `thinking*.md` / `other.md`／甚至 git 歷史中的**所有筆記、想法、文檔、程式成果**，做一次**無損壓縮**整合到本資料夾。去除了跨檔重複，但**保留每一個獨特的想法、定義、數字、理由與決策**。
>
> **整理原則**：以較新者為標準事實（newer = truth）。較舊或已被取代的內容仍完整保留，但會在檔頭明確標註其狀態與「現行對應檔」。
>
> **建立日期**：2026-06-07。本索引由主代理彙整，內容檔由 12 個並行子代理產出（共 31 個內容檔）。
>
> **原始檔未被更動**：本資料夾只「新增」整合檔；`roadmap.md`、`core_nature/`、`try_implement/` 等原始檔一律保留原狀。

---

## 1. 三大分類（檔名前綴）

| 前綴 | 類別 | 內容 |
|---|---|---|
| `note_` | **筆記與想法** | 願景、初心、隱喻、跨環境思考、歷史思考演化、決策清單、未決題、舊版設計世系 |
| `doc_` | **文檔 / 規範 / 架構** | 正式規範（九軸、library 契約、CLI、複合）、舊版架構文件、原型概念文件 |
| `code_` | **程式成果** | 正式核心 library、原型工具 / lib / demos / 測試的說明 |

> 使用者明確要求：**文檔、筆記想法、程式成果三者待在不同檔案中**。本資料夾即依此三類分檔。

---

## 2. 權威 / 新舊分層（讀任何檔前先內化）

理解這個分層，才知道哪些是「定案事實」、哪些是「歷史脈絡」：

| 層級 | 可信度 | 對應檔 |
|---|---|---|
| **① 最新事實（定案）** | 判斷「該怎麼做」的尺 | `note_01`（願景/北極星）、`note_06`（決策）、`doc_05~08`（核心規範：九軸/契約/CLI/複合）、`doc_01~04`（本質/執行形式）、`code_01`（正式核心程式碼） |
| **② 原型 / 提案（非定案）** | 可跑，但多數尚未扶正成規範 | `code_02~04`（try_implement 程式）、`doc_20~21`（馴化框架 / 組合維度概念） |
| **③ 歷史思考（已被①取代，保留供溯源）** | 理解演化脈絡用，非現況 | `note_02~05`（隱喻、跨環境、早期分層/元件思考） |
| **④ 舊版架構文件（部分已被①取代）** | 另一條設計支線，獨特設計仍有參考價值 | `doc_10~17`（原 `docs/architectures/`，以 ai-core-* CLI + FastAPI 為骨幹） |
| **⑤ 舊版設計世系（已被完全覆蓋）** | 僅供溯源，與現行衝突一律以①為準 | `note_07~09`（git 歷史 `684082c`：process hub / LISP 組合 / litellm 依賴） |

**衝突裁決規則**：① > ②/③/④/⑤。任何 ③④⑤ 與 ① 矛盾處，以 ① 為準。

---

## 3. 一句話定義（來自 note_01 / roadmap）

> ai_core = **一個讓 LLM 退縮到「意圖翻譯」與「模糊前沿」兩介面的基底；它以「拒絕為預設、憑證准入」治理 LLM 的領地，並用飛輪持續把模糊地帶固化成確定性程式碼。**

第一目標問題：**程式碼輔助助手**（上層聰明模型讀框架生資產＝結構化抽取；下層笨模型在精準標記位置填碼＝受約束生成）。

設計哲學（前提的邏輯必然，非美學）：**KISS / Lightweight / No wheel-remake / Least dependency**；硬約束=只用 Python 3.11+ 標準庫、`dependencies=[]`、目標平台 POSIX（Windows 不考慮）。

---

## 4. 建議閱讀順序

**想懂「為什麼做、做什麼」** → `note_01`（願景/北極星）→ `note_06`（決策與未決題）→ `note_02`（隱喻，幫助直覺）

**想懂「規範現況」** → `doc_01`（本質）→ `doc_03`（執行形式）→ `doc_05`（九軸）→ `doc_06`（library 契約）→ `doc_07`（CLI）→ `doc_08`（複合）

**想懂「程式做到哪了」** → `code_01`（正式核心）→ `code_02~04`（原型工具/lib/demos）

**想懂「規劃中的五大元件」** → `doc_12`（Entry Manager）/ `doc_13`（Hub）/ `doc_14`（author+SFC）（注意：④舊版架構，命名與現行不同）+ `doc_20`（馴化框架）

**想懂「演化脈絡 / 被放棄的構想」** → `note_03~05`（歷史思考）→ `note_07~09`（更早的舊版世系）

---

## 5. 完整檔案目錄

### note_ 筆記與想法

| 檔 | 狀態 | 一行摘要 |
|---|---|---|
| [note_01_vision_and_origin.md](note_01_vision_and_origin.md) | ① 定案/北極星 | 初心、演化鏈、時間維度、終點兩介面、拒絕為預設治理、第一目標、v0 切片；**附錄含使用者原話逐字** |
| [note_02_metaphors.md](note_02_metaphors.md) | ③ 歷史 | 兩大隱喻：牢籠與猴子（細線/邊界/纜線/飛輪）、滑坡軌道與球（執行形式的本質） |
| [note_03_cross_environment.md](note_03_cross_environment.md) | ③ 歷史 | 規範本質（對不確定性建模+引導缺損智能）、跨環境統一管理願景、執行狀態跨環境性 |
| [note_04_thinking_layers_model.md](note_04_thinking_layers_model.md) | ③ 歷史 | 早期：HL 基礎設施定位、用途/執行模型分類、OOP 對應、multi-shot 狀態目錄 |
| [note_05_thinking_components.md](note_05_thinking_components.md) | ③ 歷史 | 早期元件思考：Routing 家族、SFC Layer 0~4、rma 資源監控、production、pending §1-6 |
| [note_06_decisions_and_open_questions.md](note_06_decisions_and_open_questions.md) | ① 定案/活文件 | 已收斂決策、A/B/C/D 區待決、候選新軸、roadmap §7 篩子表、§8 七大開放問題 |
| [note_07_legacy_system_design.md](note_07_legacy_system_design.md) | ⑤ 舊版世系 | 舊版兩世代：抽象世代（LISP 組合/make_model→bind→take）+ process 世代（hub/AI 自我擴展） |
| [note_08_legacy_readme_usage_arch.md](note_08_legacy_readme_usage_arch.md) | ⑤ 舊版世系 | 舊版操作面：hub CLI、--metadata 約定、AI 六步擴展、planner+chain 編排、五條不變式 |
| [note_09_legacy_processes_agents.md](note_09_legacy_processes_agents.md) | ⑤ 舊版世系 | 舊版 44 個 process 全清單（含大宗 go_* Go 輔助）、AGENTS.md tag 分群 |

### doc_ 文檔 / 規範 / 架構

| 檔 | 狀態 | 一行摘要 |
|---|---|---|
| [doc_01_nature_overview.md](doc_01_nature_overview.md) | ① 規範(部分待填) | 設計模式本質定位、為何從 terminal 出發、目標平台 |
| [doc_02_terminal_model.md](doc_02_terminal_model.md) | ① 規範 | 從 terminal 出發的執行模型：命令解析、CLI 慣例、subcommand |
| [doc_03_execution_forms.md](doc_03_execution_forms.md) | ① 規範 | 執行形式（§0 九軸表 + 各形式 + 觸發機制） |
| [doc_04_data_format.md](doc_04_data_format.md) | ① 規範 | JSON 為通用格式、argv→JSON 工具 |
| [doc_05_axes_metadata.md](doc_05_axes_metadata.md) | ① 規範 | **九軸完整規範**：entries/lifecycle/state/state_dirs/resources/interruptible/guarantee/dry_run/nondeterministic |
| [doc_06_lib_contract.md](doc_06_lib_contract.md) | ① 規範 | **library 契約**（最長 829 行）：register/register_subcommand/register_subcommand_resolver/intercept |
| [doc_07_cli.md](doc_07_cli.md) | ① 規範 | CLI 即 Lisp 函式呼叫（keyword pair→dict）、argparse |
| [doc_08_composite.md](doc_08_composite.md) | ① 規範 | 複合慣例：標準狀態目錄（.config/.cache/.state/.data）、中斷恢復（recovery.json） |
| [doc_10_arch_overview.md](doc_10_arch_overview.md) | ④ 舊版架構 | 設計原則、Singleton Pattern 兩亞型、架構圖、技術選型、四元件表 |
| [doc_11_arch_protocol.md](doc_11_arch_protocol.md) | ④ 舊版架構 | 舊版 metadata 協議（io/examples/errors/dependencies/entrydata/server）、--json-errors |
| [doc_12_arch_entry_manager.md](doc_12_arch_entry_manager.md) | ④ 舊版架構 | LLM Entry Manager：queue/rate limit/timeout/streaming/token 認證 |
| [doc_13_arch_hub.md](doc_13_arch_hub.md) | ④ 舊版架構 | Function Hub：one-shot+server 雙形態、掃描策略、SFC 展開、LLM 摘要自舉 |
| [doc_14_arch_author_sfc.md](doc_14_arch_author_sfc.md) | ④ 舊版架構 | ai-core-author（generate→dry-run→register）、Small Function Center |
| [doc_15_arch_project_milestones.md](doc_15_arch_project_milestones.md) | ④ 舊版架構 | 目錄樹、CLI、config.json、里程碑 M0–M6、跨平台 |
| [doc_16_arch_agents.md](doc_16_arch_agents.md) | ④ 舊版架構 | Agent 整合、AGENTS.md 模板、export 格式、Self-Sufficiency 路徑 |
| [doc_17_arch_changelog.md](doc_17_arch_changelog.md) | ④ 舊版架構 | 舊版完整變更紀錄（2026-04-28 ~ 05-18） |
| [doc_20_taming_framework.md](doc_20_taming_framework.md) | ② 原型概念 | LLM 隨機性馴化五層框架（契約/確定化/驗證/聚合/編排）對應已實作零件 |
| [doc_21_composition_dimension.md](doc_21_composition_dimension.md) | ② 原型概念 | 組合維度（與軸正交）、軸推導規則、組合 vs 交互、blackboard 交互模型 |
| [doc_30_html_dashboard.md](doc_30_html_dashboard.md) | ④/展示 | HTML 儀表板（8 頁）；捕捉其獨特內容（HL 角色對照表、2026-05-25 現況快照、各模組進度標記），其餘交叉引用 |

### code_ 程式成果

| 檔 | 狀態 | 一行摘要 |
|---|---|---|
| [code_01_core_library.md](code_01_core_library.md) | ① 正式核心 | `src/ai_core/_core.py` API、九軸驗證、pyproject、測試覆蓋（test_core.py = 65 passed） |
| [code_02_prototype_tools.md](code_02_prototype_tools.md) | ② 原型 | indexer/router/switch/sfc/hub/llm_entry_manager/chain 各工具職責、CLI、狀態 |
| [code_03_prototype_lib.md](code_03_prototype_lib.md) | ② 原型 | lib/ 11 模組：state_dirs/recovery/memoize/server/singleton/trace/call/llm_call/compose/interact/compose_meta |
| [code_04_prototype_demos_tests.md](code_04_prototype_demos_tests.md) | ② 原型 | 3 個 demo、兩套煙霧測試（72+68=140 全綠）、範例函式、store seed |

---

## 6. 整合時發現的衝突與校正（重要）

子代理在無損整合過程中發現以下**跨檔不一致**。一律記錄，不擅自抹平；現況事實以此處裁決為準：

1. **測試數字三套並存（非矛盾，是不同範圍）**：
   - `tests/test_core.py` = **65 passed**（正式核心 register/intercept/九軸；已實機確認，見 `code_01`）。
   - `pytest -q` 全收集 = **82 passed**（CLAUDE.md 數字；含 `tests/` 下其他測試檔，差額 17）。
   - 原型煙霧測試 = `smoke_test.py` **72** + `lib_smoke_test.py` **68** = **140**（已實機確認，見 `code_04`）。
   - → 三個數字都對，指的是不同東西。引用時請註明範圍。

2. **「八軸」vs「九軸」**：`roadmap.md`（含使用者原話）多處仍寫「八軸」，但現行定案已是**九軸**——第九軸 `nondeterministic` 於 2026-05-26 扶正（承載「拒絕為預設+憑證准入」治理）。`note_01` 依無損保留 roadmap 的「八軸」原措辭；**現行事實以 `doc_05` / `code_01` 的九軸為準**。

3. **舊版架構（④）根本沒有「軸」概念**：`doc_10~17`（原 docs/architectures）用的是扁平 metadata key（name/summary/io/examples/errors/dependencies…），**連八軸都沒有**，是比 roadmap 更早的另一條設計支線；且引入 litellm/fastapi/uvicorn 等外部相依，與現行「零相依」硬約束衝突。詳細衝突清單見 `doc_11` 與 `doc_17` 檔頭。

4. **舊版世系（⑤）與現行的戲劇性反轉**：`note_07~09` 的舊版**明確拒絕**常駐 LLM server / queue / rate limit / SFC dispatcher——而這些正是現行**規劃中的五大元件**。舊版核心隱喻「LLM 即 tokens→tokens 純函數 + AI 自我擴展門檻越低越好」也與現行「LLM 退縮為兩介面 + 拒絕為預設」相反。同名 `core.py` 但與現行 `_core.py` 完全無關（舊版依賴 litellm）。

5. **執行狀態術語落差**：`other.md`（③）用 one-shot/multi-shot/persistent/singleton 四態；現行 `lifecycle` 軸只有 `one_shot`/`persistent` 兩值，`state` 軸用 `stateless`/`stateful_external`。對應關係：multi-shot→`state` 軸的有狀態、singleton→「重量級函式變 server / 單例資源」概念。

6. **早期 `register()` 語意已漂移**：`thinking_oop`（③）的 `register()` 兼做 registry 登記 + CLI dispatch 生成、且「只能呼叫一次否則 raise」；現行 `register()` 是**純宣告、零副作用、last-write-wins**。勿混淆。`Function(ABC)` 繼承路線亦已廢棄，改純宣告函式。

7. **MEMORY.md 索引已過時**：使用者的自動記憶 `MEMORY.md` 仍指向 `SYSTEM_DESIGN*.md` / `README.md` / `MICROSERVICES_GUIDE.md` 等——這些是**舊版世系**檔，已於 commit `7b9b8e3` 被覆蓋、不在工作目錄。其內容已由本知識庫 `note_07~09` 無損保留。（`MICROSERVICES_GUIDE.md` / `SHELL_FUNCTIONS_GUIDE.md` 兩檔在 git 歷史 `684082c` 樹中未出現，疑為更早被刪或從未提交；若使用者確知其存在，需另行指認 commit。）

---

8. **HTML 儀表板（已處理）**：repo 內有一組 8 頁的展示型網站（`overview.html` + `html/*.html` + `_shared.css`），是把 roadmap/core_nature/DECISIONS/docs 重編排成的 dark-theme 儀表板。其文字內容絕大多數是既有 markdown 的呈現版；獨特部分（元件→Heuristic Learning 角色對照表、2026-05-25 現況快照數據、各 `src/ai_core/` 子模組的完成/最小/YAGNI 進度標記、頁尾索引）已無損捕捉於 `doc_30`。注意：儀表板停在較早狀態——全程稱「**8 軸**」並把 `nondeterministic` 當「候選 C1」（現行為 9 軸）、且 D 組決策數內部不一致（以 `note_06` 為準）。`_shared.css` 為純樣式、無文字內容，未整合。

9. **未整合的零星檔（均為非內容性或已被概念涵蓋，列此存查）**：
   - `progress.md`：僅一行 `claude --resume <uuid>` 的 session 續接指標，無設計內容，不整合。
   - `funcs/echo.sh`：頂層範例函式，但實為**骨架 stub**（函式體幾乎全是註解，描述「解析旗標 / `--metadata` 模式印 §4.5 JSON / 一般執行寫 input→output」之意圖，尚未實作完整）；其 `--metadata` 協議範例意涵已由 `doc_11`（協議）與 arch echo.sh metadata 範例涵蓋。
   - `routes.json` / `switch.json` / `chain_demo.json` / `store/functions.json` / `store/index.json`：原型的設定/種子資料，其結構與內容已於 `code_02`（routes/switch/chain）與 `code_04`（store seed：shout / wc_words）說明。
   - `CLAUDE.md`：專案給 AI 的工作指引（非待整合的筆記/文檔），維持原狀作為 repo 入口；見下節。
   - `pyproject.toml`：打包設定，已於 `code_01` 說明（hatchling、`dependencies=[]`、`>=3.11`、`[dev]`）。

---

## 7. 與原始 repo 的關係

- 本知識庫是**整合視圖**，原始檔（`roadmap.md`、`core_nature/`、`try_implement/`、`docs/`、`thinking*.md`、`other.md`）仍是各自領域的「正本」，未被更動。
- 若日後要以本知識庫**取代**散落的原始檔，可逐類驗證後再刪除原檔——但這需使用者明確指示，本次未執行。
- `CLAUDE.md`（專案給 AI 的工作指引）仍為 repo 的入口說明，與本知識庫並行；本知識庫補足「把所有散落筆記收攏在一處」這件 CLAUDE.md 未涵蓋的事。
