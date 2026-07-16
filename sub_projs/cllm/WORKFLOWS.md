# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜技術入口 [README.md](README.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。

## 你想做什麼 → 用哪個工作流

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「我想開發／修改功能」「加旗標／加 vcpkg 依賴」「**修 bug**」 | **feature-dev** | [workflows/feature-dev/README.md](workflows/feature-dev/README.md) |
| 「跑測試／驗證」「建置」 | **testing** | [workflows/testing.md](workflows/testing.md) |
| 「**記／查踩坑**」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |
| 「上手／查用法／C ABI／CLI／接真後端」 | **（非工作流）** | [README.md](README.md) |

> 常見工作流菜單，**需要哪個才加哪列**（入口檔在第一次用到時才建，從單檔開始長——見 [DEV-GUIDE](DEV-GUIDE.md) 四級成長軌跡）：refactor（重構／拆檔）、investigation（調查／可行性）、spec（把 idea 討論成方案）、plan（把方案展開成計畫）、dev-env（開發環境、換機、打包出貨）。

**都不符 → 看 [INDEX.md](INDEX.md)**（repo 頂層結構地圖）。

## 工作流的統一形式（規範）

所有工作流照同一套形式（細則見 [DEV-GUIDE](DEV-GUIDE.md)）：

**檔名規範**：
- **README** = 初入一個資料夾**先讀的入口／導引**。
- **INDEX** = **描述該資料夾頂層結構**的索引。
- 小資料夾兩者可合一；大到結構複雜時才分出獨立 INDEX。

形式：
- **資料夾型工作流**：一個**入口 README**（或主檔）＋ **`archive/`**（過時文檔封存、不在維護鏈）＋視需要的 `gotchas.md` / `session-log.md`。
- **單檔工作流**：一個 `.md` 同時是入口與內容；撐大了就照[結構整理原則](DEV-GUIDE.md)升級成資料夾型。

## 跨工作流的活狀態（repo 根）

- **進度**（還沒完成的 in-flight / open）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做／驗證的**（實機環境／外部服務／權限）→ [WAIT_USER.md](WAIT_USER.md)
