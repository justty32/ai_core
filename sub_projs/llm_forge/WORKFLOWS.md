# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

本子專案**現在處於規劃期**（把 idea 討論成設計方案），派發表反映實況、保持薄。

## 你想做什麼 → 用哪個工作流

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「把某個 idea 討論成設計方案」（**現階段主戰場**）| **spec** | 第一次用到才建（從單檔起）；礦脈見下方規劃管線 |
| 「把設計方案展開成動工計畫」 | **plan** | 第一次用到才建（從單檔起）|
| 「記 / 查踩坑」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |
| 「開發 / 修改功能」「修 bug」 | **feature-dev** | **尚未開工**（無程式碼）；範例入口 [workflows/feature-dev/README.md](workflows/feature-dev/README.md) 留檔待用 |
| 「跑測試 / 驗證」 | **testing** | **尚未開工**（無程式碼）；範例入口 [workflows/testing.md](workflows/testing.md) 留檔待用 |

**都不符 → 看 [INDEX.md](INDEX.md)**（repo 頂層結構地圖）。

## 規劃管線（一個想法的成熟過程）

保持這條管線：**idea（要不要做？）→ spec（討論後方案）→ plan（動工前詳規）→ build（feature-dev）**。

- **idea 階段的礦脈在主專案** `ai_core/workflows/ideas/notes/`——尤其 [20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](../../workflows/ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)（本子專案的唯一內容來源，標〔待續〕，還會被後續討論擴充）。
- **spec / plan 入口檔第一次用到才建**（從單檔開始長，見 [DEV-GUIDE](DEV-GUIDE.md) 四級成長軌跡）——**不預先建空資料夾**。

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
