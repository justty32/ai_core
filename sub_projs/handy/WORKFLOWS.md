# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

## 你想做什麼 → 用哪個工作流

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「**加一個新住戶／寫個小腳本**包裝某能力」 | **new-resident** | [workflows/new-resident.md](workflows/new-resident.md) |
| 「用 llme 換 endpoint／加一個 endpoint config」 | **llme** | [llme/README.md](llme/README.md) |
| 「調 zhtw／照 zhtw 做一個新人格薄包裝」 | **zhtw**（範例住戶）| `zhtw` 檔頭註解 |
| 「**記 / 查踩坑**」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |
| 「開發環境 / 後端 / build / fresh clone 要做什麼」 | **dev-env** | [workflows/dev-env.md](workflows/dev-env.md) |

**都不符 → 看 [INDEX.md](INDEX.md)**（repo 頂層結構地圖）。

> 新工作流入口檔**第一次用到才建**，從單檔開始長（見 [DEV-GUIDE](DEV-GUIDE.md) 四級成長軌跡）。常見可能新增：refactor（拆檔/整理）、investigation（解讀外部系統）、tooling（外部工具/env）。

## 定期喚醒（kernel 內建，與上面派發表分開）

一套定期工作流。**兩種進入**：
1. **循環執行**：[`/wf-tick`](.claude/commands/wf-tick.md) 每隔週期喚醒 tick → tick 派發下面各工作流，判時間、**做**到期項。
2. **使用者登記**：你直接請求 →「**幫我登記行程**」進 schedule、「**加個常規事務**」進 routines，只寫進清單、不當場做（等 tick 到點才做）。

| 工作流 | 入口 | 做什麼 |
|--------|------|--------|
| **tick** | [workflows/tick.md](workflows/tick.md) | 定期心跳的**單次**執行——當**派發器**依序跑各定期工作流（routines → schedule）。由 `/wf-tick [週期]` 喚醒。 |
| **routines** | [workflows/routines.md](workflows/routines.md) | **固定例行**清單：判當地時間（Asia/Taipei）→ 對照時機分區/間隔登記表 → 到期的唯讀事務就做。 |
| **schedule** | [workflows/schedule.md](workflows/schedule.md) | **一次性**定時請求：判時間 → 到點就做、做完刪。 |

**tick 只派發、不判斷**；「什麼時間該做什麼」的判斷與清單各歸 routines / schedule。

## Phase 2：用 handy 執行檔復刻 workflows 功能（規劃中）

[`~/repo/workflows`](/home/lorkhan/repo/workflows) 那套是**給人開的 Claude Code 照著跑的 markdown 提示**。handy 的進一步目標是**用可執行工具集復刻其功能**——做成能跑的住戶：

- **`wf`**（規劃中）：`./wf <任務>`（如 `./wf 幫我去改這段程式`）→ 把任務**派發給 claude code 這類成熟 agent** 跑（dispatch-by-intent，但可執行）。
- **inbox 協議**：太複雜的任務走 email 式交接給更成熟的 agent（見 workflows repo `template/workflows/inbox`）——由 `wf` 在複雜任務時採用。

> 上面的 markdown 定期工作流（tick/routines/schedule）是**現在就能用的、給 agent session 照著跑的形式**；`wf` 執行檔是**未來讓這些功能變裸命令**的方向，兩者並存不衝突。進度見 [SESSION-LOG](SESSION-LOG.md)。

## 工作流的統一形式（規範）

**檔名規範**：README＝入口/導引；INDEX＝頂層結構索引。小資料夾兩者合一，大了才分。

形式：
- **單檔工作流**（多數停在這）：一個 `.md` 同時是入口與內容；撐大了照 [DEV-GUIDE 結構整理原則](DEV-GUIDE.md)升級成資料夾型。
- **資料夾型工作流**：一個入口 README（或主檔）＋視需要的 `archive/`（封存過時文檔）、`gotchas.md`、`session-log.md`。
- 入口檔本身膨脹 → 一樣照結構整理原則拆。

**住戶（llme / zhtw…）**也是工作流的一種：資料夾型住戶（llme）入口是 `README.md`，單檔住戶（zhtw）入口是檔頭註解。到底有哪些住戶看 [INDEX](INDEX.md)。

## 跨工作流的活狀態（handy 根）

- **進度**（我自己 open 的 in-flight）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做 / 驗證的**（實機/外部服務/env/權限）→ [WAIT_USER.md](WAIT_USER.md)
