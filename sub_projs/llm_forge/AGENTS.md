# llm_forge — AI agent 專案備忘

llm_forge = **把 LLM 鍛成可靠管線的框架（爐子）——固化階梯／雙錨驗證／評分級聯／世界模型↔context 等機制的家；ai_core 北極星「把小笨模型包裝成可靠函式」在 galgame 線上的框架落點**。本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 與 galtxt 的關係（本專案定位）

```
galtxt（實驗場，先跑通）──機制確定後──▶ llm_forge（本專案：框架/爐子，固化收納）
                                              ▲ 依賴
```

**llm_forge＝機制之家，不是 galgame 生成器本身。** 生成器（第一個實際應用）是 [galtxt/](../galtxt/AGENTS.md)——它**依賴**本專案的機制、在其上做 galgame 台詞生成；**在 galtxt 跑通、被使用者確認的機制才畢業搬來這裡固化收納**。llm_forge 不憑空立法造框架，框架從 galtxt 的實戰結晶而來。

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

1. **先執行再說，邊執行邊生規範**——框架規範從 galtxt 的執行裡挖出、不開工前立法；**不搞 spec-first 儀式**（別再複製「九軸 N 輪會議才寫碼」那套）。
2. **只固化「在 galtxt 跑通確認」的機制**——別在 llm_forge 憑空造框架；機制先在 [galtxt/](../galtxt/AGENTS.md) 實戰，確認後才搬來這裡結晶成可重用框架。
3. **能跑的用「搬（port）」、別憑記憶重建**——LLM 接口地基（core_handy 的 `llm_entry` 那條線）已能跑，要用就 port，別憑記憶重造（否則落回「一直重來」）。
4. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
5. **改動使任何一層文檔失準時，同步更新該層**——尤其本 [INDEX.md](INDEX.md) 與 [WORKFLOWS.md](WORKFLOWS.md)，以及上層 [sub_projs/README.md](../README.md)、[ai_core INDEX](../../INDEX.md)。
6. **所有回覆、註解、留檔用繁體中文**；程式碼識別子、shell 指令、技術名詞保留原文。

## 開發環境

技術棧方向〔§9.2 決定/方向〕：主力 **C++ 效能核心 ＋ 內嵌 Lua VM 當通用腳本層**（Fennel 編譯成 Lua ⇒ 使用者手寫用 Lisp/Fennel、同 runtime）；**少 Python（太慢）、少 bash**。語言中立的縫＝**LLM 的 socket 協定 ＋ 函式的 `--metadata` 文字契約**。

**落腳與畢業**〔§9.5〕：框架日後可能整包**畢業成獨立 repo**（port core_handy 的能跑地基過去），現階段先在 `ai_core/sub_projs/llm_forge/` 落腳；動手實戰在隔壁 [galtxt/](../galtxt/AGENTS.md)。

**驗證**：目前**尚無**可跑驗證指令（規劃期、無程式碼）；有第一片程式碼後再於 [workflows/testing.md](workflows/testing.md) 補上。

## 主工作流（進度與待測）

事情告一段落、因應需求結束、或臨時中止時 → 把**還沒完成**的活狀態記到進度；需要**使用者親自做／驗證**的（實機環境、外部工具實跑、需權限/本機環境）→ 記到待使用者。兩者都**只列 open**，完成即移除、不留已完成清單。

- **進度** → [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者** → [WAIT_USER.md](WAIT_USER.md)
