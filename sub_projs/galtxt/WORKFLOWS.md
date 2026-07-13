# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

galtxt 是**執行優先**的實驗場（鐵律：先執行再說），故主戰場是 feature-dev / testing；規劃類的討論在上游 idea 礦脈，不在本子專案複製。

## 你想做什麼 → 用哪個工作流

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「開發 / 修改功能」「**修 bug**」（**主戰場**）| **feature-dev** | [workflows/feature-dev/README.md](workflows/feature-dev/README.md)（尚未開工，範例骨架留檔待用）|
| 「跑測試 / 驗證」 | **testing** | [workflows/testing.md](workflows/testing.md)（尚無指令）|
| 「**記 / 查踩坑**」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |

需要新工作流（refactor / investigation / spec / plan…）**第一次用到才加一列**，入口檔從**單檔**開始長（見 [DEV-GUIDE](DEV-GUIDE.md) 四級成長軌跡），不預先建空資料夾。規劃管線的上游討論在 idea 礦脈（見 [INDEX](INDEX.md)「內容源頭」）。

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

- **進度**（還沒完成的 in-flight / open）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做 / 驗證的**（實機環境 / 外部工具 / env / 權限）→ [WAIT_USER.md](WAIT_USER.md)
