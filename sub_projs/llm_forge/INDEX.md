# INDEX — llm_forge 專案地圖

整個專案的頂層導航。llm_forge = **galgame 台詞生成器——用固化階梯把台詞產出從純 LLM 一步步鍛成「笨模型＋鷹架＋護欄」的可靠管線，ai_core 北極星的第一個目標問題**。AGENTS.md 只放主工作流 + 指向本檔；細節從這裡分流出去。

---

## Repo 佈局

現階段只有規劃期文檔骨架＋工作流目錄，**尚無程式碼**（第一片生成切片拉動時才長出產出目錄）。

| 路徑 | 內容 |
|------|------|
| `AGENTS.md` / `CLAUDE.md` / `WORKFLOWS.md` / `INDEX.md` / `DEV-GUIDE.md` | 頂層路由 / 轉址 / 派發 / 地圖 / 結構整理參考 |
| `SESSION-LOG.md` / `WAIT_USER.md` | 活狀態（open-only）：進度 hub、待使用者項 |
| `workflows/` | 開發工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）|

## 開發工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)**——依「你想做什麼」派發。每個工作流的 durable 知識歸在 `workflows/<該工作流>/`（入口＝該夾 README 或主檔，含 `archive/` 封存過時文檔），具體流程在各自入口檔。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**（結構整理原則 + 四級成長軌跡）——**只在要重構/整理結構時取用**。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)；碰原始碼的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[gotchas](workflows/common/gotchas.md) 踩坑 + [conventions](workflows/common/conventions.md) 程式碼慣例 |

## 規劃來源

本子專案的內容源頭在主專案，不在本目錄複製：

- **idea 礦脈**：[../../ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](../../ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)——唯一內容來源（標〔待續〕，會被後續討論擴充）。
- **未來 port 的地基**：[../core_handy/notes/00_index.md](../core_handy/notes/00_index.md)——C++ 的 LLM 接口地基（`llm_entry.cpp` + `impl/{http,json,llm,rate,serve}.hpp`），畢業獨立 repo 時搬過去（〔§9.5 決定〕）。

## 活狀態（只列還沒完成的）

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根）→ 各工作流 session-log（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證的入口（repo 根；膨脹後拆 `wait_todo/` 分類檔）|
