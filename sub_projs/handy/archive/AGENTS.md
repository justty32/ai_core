# handy — AI agent 專案備忘

handy = **路徑一（把 OS 當 AI agent）的落地試驗田——一組靈活的小腳本／小程式集，拿現成程式（尤其 [cllm](../cllm/core/AGENTS.md) 與其 tool）用腳本包裝、按慣例組合**。北極星＝**整個作業系統變成一個 AI agent**（工具集，非單體）。本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 先讀哪裡

- **使用者要你動手做某件事** → **[WORKFLOWS.md](WORKFLOWS.md)**：依意圖派發到對應工作流，再讀該工作流入口。
- **想看專案長怎樣 / 有哪些住戶 / 設計脈絡** → **[INDEX.md](INDEX.md)**：repo 頂層結構地圖。
- **要用 cllm／cllm tool** → [cllm/AGENTS.md](../cllm/core/AGENTS.md)（重點現成件清單見 [INDEX](INDEX.md)）。

## 分層思想（本專案的組織原則）

整個 handy 是一棵**分層樹**，每層**只指向下一層、不存下層細節**：

```
AGENTS.md（本檔，最頂）→ WORKFLOWS.md / INDEX.md → 各住戶/工作流入口 → 內容 → 子工作流…
```

- **README**＝初入一個資料夾**先讀的入口／導引**；**INDEX**＝**描述該資料夾頂層結構**的索引。小資料夾兩者合一，大了才分。
- **durable 知識歸到它所屬的那一層／那個住戶**，絕不往上堆——所以 AGENTS.md 才這麼薄。要某主題細節，順著樹往下走，不在本檔找。
- **[DEV-GUIDE.md](DEV-GUIDE.md) 是被動參考**（結構整理原則＋四級成長軌跡）——**只在要重構/整理結構時取用**，不貫穿日常。
- **改動使任一層文檔失準時同步更新**——本檔、[INDEX](INDEX.md)、上層 [sub_projs/README.md](../README.md)、[ai_core INDEX](../../INDEX.md)。

## 鐵律（always-on，任何時候都遵守）

1. **這是試驗田，不是基礎建設**——低風險、隨時可推倒；守「這是玩、不是一次蓋對」，才不落回「可以永遠做下去卻沒消費者」的 yak-shave。
2. **拿現成用腳本包**——優先 port／wrap 現成件（清單見 [INDEX](INDEX.md)「不重造」），別憑記憶重造。
3. **先執行再說，邊做邊生慣例**——config 慣例、poll「已讀」形狀等，實作時遇到再定，不開工前立法。
4. **重構/整理不改原意**——改完跑驗證（`./zhtw …`／`llme deepseek …` 真後端，或 `file://` fixture）。
5. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
6. **全繁體中文**（回覆、註解、留檔）；程式碼識別子、shell 指令、技術名詞保留原文。

> 鐵律只放「任何時刻都適用」的；只在特定場景適用的規矩下放到該工作流。各住戶/工作流的**具體流程在它自己的入口檔**，不在本檔。

## 主工作流（活狀態：進度 / 待使用者）

事情告一段落或臨時中止時 → 把**還沒完成**的活狀態記到進度；需**使用者親自做／驗證**的 → 記到待使用者。兩者**只列 open**，完成即移除、不留已完成清單（歷史交給 git log）。

- **進度**（我自己 open 的 in-flight）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者**（等使用者親自做/驗證）→ [WAIT_USER.md](WAIT_USER.md)

> 開發環境（技術棧、cllm 依賴、後端、build、daemon 觸發條件）在 [workflows/dev-env.md](workflows/dev-env.md)。跨住戶踩坑在 [workflows/common/gotchas.md](workflows/common/gotchas.md)。
