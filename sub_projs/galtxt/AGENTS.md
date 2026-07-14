# galtxt — AI agent 專案備忘

galtxt = **galgame 台詞生成的動手實驗場——把台詞產出從純 LLM 一步步鍛成「笨模型＋鷹架＋護欄」的可靠管線，ai_core 北極星第一目標問題的落地執行處**。本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 與 llm_forge 的關係（本專案定位）

```
galtxt（實驗場，先跑通）──想法確定後──▶ llm_forge（框架/爐子，固化收納）
   依賴 ↓
llm_forge 的機制（固化階梯／雙錨驗證／評分級聯／世界模型↔context…）
```

**galtxt＝先執行的實驗場，不是規範定案處。** 想法先在這裡跑通、被確認，**才搬進 [llm_forge/](../llm_forge/AGENTS.md)** 固化成可重用框架；反過來 galtxt **依賴** llm_forge 的機制，但機制雛形常先在這裡長出來。

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
- **[DEV-GUIDE.md](DEV-GUIDE.md) 是被動參考**（結構整理原則 + 四級成長軌跡）——**只在你要重構/整理結構時才取用**，不貫穿日常每個動作。只在**碰原始碼**時適用的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md)。

## 鐵律（always-on，任何工作流任何時候都遵守）

1. **先執行再說，邊執行邊生規範**——規範從執行裡挖出、不開工前立法；**不搞 spec-first 儀式**（別複製「九軸 N 輪會議才寫碼」那套）。
2. **能跑的用「搬（port）」、別憑記憶重建**——LLM 接口地基已能跑（core_handy 的 `llm_entry` 那條線），要用就 port，別憑記憶重造（否則落回「一直重來」）。
3. **確定的東西往 llm_forge 搬**——在 galtxt 跑通、被使用者確認的機制，畢業去 [llm_forge/](../llm_forge/AGENTS.md) 固化；galtxt 只留「還在試」的。
4. **改動程式碼後跑驗證確認行為不變**（快速驗證指令見 [workflows/testing.md](workflows/testing.md)；目前尚無程式碼、無指令）；**未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
5. **改動使任何一層文檔失準時，同步更新該層**——尤其本 [INDEX.md](INDEX.md) 與 [WORKFLOWS.md](WORKFLOWS.md)，以及上層 [sub_projs/README.md](../README.md)、[ai_core INDEX](../../INDEX.md)。
6. **所有回覆、註解、留檔用繁體中文**；程式碼識別子、shell 指令、技術名詞保留原文。

## 開發環境

技術棧方向〔idea 礦脈 §9.2〕：主力 **C++ 效能核心 ＋ 內嵌 Lua VM 當通用腳本層**（原案 Fennel 編成 Lua；**實際：Fennel 放棄——想玩 Lisp 走 s7 線 try_1，try_2 純 Lua**）；**少 Python（太慢）、少 bash**。語言中立的縫＝**LLM 的 socket 協定 ＋ 函式的 `--metadata` 文字契約**。

**⚠️ 本機是 Windows**：core_handy 的 daemon 用 Unix domain socket（`sys/un.h`），直接 build 會卡——port 時要嘛先只搬 one-shot 那半、要嘛換 TCP。這是實機約束，動手前先確認。

**內容源頭（唯一）**：idea 礦脈 [../../workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](../../workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)——固化階梯、雙錨驗證、評分級聯、世界模型↔context、技術棧全在此。**未來 port 的地基**：[../ver_1/try_implement/core_handy/](../ver_1/try_implement/core_handy/)（`llm_entry.cpp` + `impl/{http,json,llm,rate,serve}.hpp`）。

## 主工作流（進度與待測）

事情告一段落、因應需求結束、或臨時中止時 → 把**還沒完成**的活狀態記到進度；需要**使用者親自做／驗證**的（實機環境、外部工具實跑、需權限/本機環境）→ 記到待使用者。兩者都**只列 open**，完成即移除、不留已完成清單。

- **進度** → [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者** → [WAIT_USER.md](WAIT_USER.md)
