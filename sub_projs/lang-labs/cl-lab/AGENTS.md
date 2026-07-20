# cl-lab — AI agent 專案備忘

cl-lab = **Common Lisp（SBCL）開發環境試驗場：一個能跑的 ASDF 專案 + 一整套中文教學（docs / examples / 單頁速查）**。本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 先讀哪裡

- **使用者要你動手做某件事** → **[WORKFLOWS.md](WORKFLOWS.md)**：依使用者意圖派發到對應工作流，再讀該工作流入口。
- **想看專案長怎樣** → **[INDEX.md](INDEX.md)**：repo 頂層結構地圖。

## 分層思想（本專案的組織原則）

整個 repo 是一棵**分層樹**，每一層**只指向下一層、不存下層的細節**：

```
AGENTS.md（本檔，最頂）→ WORKFLOWS.md / INDEX.md → 各工作流入口 → 工作流內容 → 子工作流…
```

- **README**＝初入一個資料夾**先讀的入口／導引**；**INDEX**＝**描述該資料夾頂層結構**的索引。小資料夾兩者合一，大了才分出獨立 INDEX。
- **durable 知識歸到它所屬的那一層／那個工作流**，絕不往上堆——所以 AGENTS.md 才這麼薄。要某主題的細節，順著上面的樹往下走，不在本檔找。
- **鐵律（always-on，任何工作流任何時候都遵守）**：
  1. 重構/整理必須**不改變原意**（開發：行為不變，改完跑驗證 `scripts/run.sh test`（fiveam）／`scripts/run.sh build`；非開發：內容原意不變、對照 `Done when:` 驗收）。
  2. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
  3. 各工作流的**具體流程在它自己的入口檔**，不在頂層。
- **[DEV-GUIDE.md](DEV-GUIDE.md) 是被動參考**（結構整理原則 + 四級成長軌跡）——**只在你要重構/整理結構時才取用**，不貫穿日常每個動作。只在**碰原始碼**時適用的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md);只在**產出給人讀的文字**時適用的**寫作風格**在 [common/writing](workflows/common/writing.md)。本專案兩份都合了（開發 + 知識 flavor）。

## 開發環境

SBCL 2.6.6 + Quicklisp（系統既有）。常用依賴 jzon / clingon / cffi / fiveam / swank 已裝。指令一律走 `scripts/run.sh`（run / test / build / repl / swank）。環境與工具鏈細節見 [docs/00-環境與工具鏈.md](docs/00-環境與工具鏈.md)。

## 主工作流（活狀態：進度 / 待測 / 信件）

事情告一段落、因應需求結束、或臨時中止時 → 把**還沒完成**的活狀態記到進度；需要**使用者親自做／驗證**的（實機環境、外部工具實跑、需權限/本機環境）→ 記到待使用者。兩者都**只列 open**，完成即移除、不留已完成清單。

- **進度**（我自己的 open in-flight）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者**（等使用者親自做/驗證）→ [WAIT_USER.md](WAIT_USER.md)
- **信件**（agent 之間的訊息交換，像 email；放信處是 repo 根的 `inbox/`）→ 使用方式見 [workflows/inbox/](workflows/inbox/README.md)

> 三軸各管一種「還沒完的事」：進度＝我手上的、待使用者＝卡在人、信件＝agent 之間收發（寄失敗/不回都無妨）。使用者說「**看看信箱**」＝掃自己的 inbox 待辦。
