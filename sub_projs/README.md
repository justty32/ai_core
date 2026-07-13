# sub_projs — 次級物（子專案／封存）

← [INDEX](../INDEX.md)

本夾收各自成一攤的次級物：**半封存的舊實作版** `ver_1/`、**框架規劃地** `llm_forge/`、**動手實驗場** `galtxt/`。各目錄自帶入口，先讀入口再深入。

| 目錄 | 內容 | 入口 |
|------|------|------|
| `ver_1/` | 早期 Python 實作版的**封存**（src／tests／原型／範例）。已從主線退下，半封存狀態、不在現役維護鏈、不精整。 | （封存，不另設入口）|
| [llm_forge/](llm_forge/) | **框架規劃地（爐子）**：把 LLM 鍛成可靠管線的機制之家——固化階梯／雙錨驗證／評分級聯（套分層工作流模板）。目前僅規劃期文檔骨架，無程式碼。 | [AGENTS.md](llm_forge/AGENTS.md) |
| [galtxt/](galtxt/) | **動手實驗場（第一把刀）**：ai_core 北極星第一目標問題的落地執行處。**依賴 llm_forge**，先在此跑通想法、確定了才搬進 llm_forge 固化。套 workflows 分層模板，剛開、無程式碼。 | [AGENTS.md](galtxt/AGENTS.md) |
