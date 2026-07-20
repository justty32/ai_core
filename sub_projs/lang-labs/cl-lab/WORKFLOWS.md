# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

## 你想做什麼 → 用哪個工作流

本專案是**混合型**（Common Lisp 程式開發 + 中文教學/研讀），兩個 flavor 都合了。先看你的意圖落在哪張表。

### 開發 flavor

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「我想開發 / 修改某個功能」「**修 bug**」 | **feature-dev** | [workflows/feature-dev/README.md](workflows/feature-dev/README.md) |
| 「跑測試 / 驗證」 | **testing** | [workflows/testing.md](workflows/testing.md) |
| 「**記 / 查踩坑**」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |

碰原始碼的工作流共用 [common/conventions](workflows/common/conventions.md)（程式碼慣例 + code map）。

### 知識工作 flavor

規劃、寫作、閱讀消化、學習、決策、整理——產出文字的工作流共用 [common/writing](workflows/common/writing.md)（文風）。

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「寫一篇東西：文章 / 筆記 / 文件 / 翻譯 / 貼文」 | **write** | [workflows/write.md](workflows/write.md) |
| 「讀長文 / 影片 / 一堆資料，做摘要與索引」 | **digest** | [workflows/digest/README.md](workflows/digest/README.md) |
| 「規劃一件事：活動 / 旅行 / 流程 / 任意非開發專案」 | **plan-a-thing** | [workflows/plan-a-thing.md](workflows/plan-a-thing.md) |
| 「在幾個選項間做決定」 | **decide** | [workflows/decide.md](workflows/decide.md) |
| 「學一個主題，建立可延續學習筆記」 | **learn** | [workflows/learn.md](workflows/learn.md) |
| 「整理一堆資訊 / 檔案 / 筆記的結構」 | **organize** | [workflows/organize.md](workflows/organize.md) |

「記 / 查踩坑」→ [common/gotchas](workflows/common/gotchas.md)（kernel 內建，兩個 flavor 共用）。

**都不符 → 看 [INDEX.md](INDEX.md)**（repo 頂層結構地圖）。

## 工作流的統一形式（規範）

所有工作流照同一套形式（細則見 [DEV-GUIDE](DEV-GUIDE.md)）：

**檔名規範**：
- **README** = 初入一個資料夾**先讀的入口／導引**（這資料夾在幹嘛、怎麼用）。
- **INDEX** = **描述該資料夾頂層結構**的索引（有哪些子項、各放什麼）。
- 小資料夾兩者可合一（README 兼述結構）；大到結構複雜時才分出獨立 INDEX。

形式：
- **資料夾型工作流**：
  - 一個**入口 README**（或主檔）——先讀它就知道這工作流在幹嘛、有哪些檔。
  - **`archive/`**：過時 / 被取代的文檔封存於此（保留脈絡、不在維護鏈）。
  - 視需要的 `gotchas.md`（踩坑）、`session-log.md`（本工作流 open 進度）。
- **單檔工作流**（還沒長成資料夾的那些）：一個 `.md` 同時是入口與內容；撐大了就照「[結構整理原則](DEV-GUIDE.md)」升級成資料夾型。到底有哪些工作流、各自入口在哪，看上面的派發表即可，不在這裡逐一點名（正面清單每次升級都會過期）。
- 入口檔本身膨脹 → 一樣照結構整理原則拆。

## 跨工作流的活狀態（repo 根）

三軸各管一種「還沒完成的事」，都**只列 open**、完成即移除：

- **進度**（我自己還沒完成的 in-flight / open）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做 / 驗證的**（實機環境 / 外部工具 / env / 權限）→ [WAIT_USER.md](WAIT_USER.md)
- **信件**（agent 之間的訊息交換，像 email——寄失敗/不回都無妨；放信處是 repo 根的 `inbox/`）→ 使用方式見 [workflows/inbox/](workflows/inbox/README.md)（可選；單方專案不需要就整包刪）
