# sub_projs — 真・半獨立子專案

← [INDEX](../INDEX.md)

本夾收**與主線（`workflows/` 的 roadmap＋ideas＋spec）鬆耦合、各自成一攤的次級物**：一是**封存的舊實作版** `ver_1/`，二是**有自己生命週期的半獨立子專案** `llm_forge/`；放這裡讓頂層清爽。各目錄自帶入口，先讀入口再深入。

| 目錄 | 內容 | 入口 |
|------|------|------|
| [ver_1/](ver_1/try_implement/README.md) | **封存的最早 Python 實作版**：`src/`（`ai_core` 核心）＋`tests/`＋`try_implement/`（原型遊樂場，含 [core_handy/](ver_1/try_implement/core_handy/) C++ 原型地基）＋`examples/`＋`pyproject.toml`。整套已從主線降級，之後**只當參考、不精整**。 | [try_implement/README.md](ver_1/try_implement/README.md) |
| [llm_forge/](llm_forge/) | **galgame 台詞生成器規劃地**：ai_core 北極星的第一目標問題，固化階梯／雙錨驗證／評分級聯（套分層工作流模板）。目前僅規劃期文檔骨架，無程式碼。 | [AGENTS.md](llm_forge/AGENTS.md) |

## 其餘原住戶去了哪（結構重整後）

本夾原先還混著知識庫快照、展示產物、原型地基等非子專案物；重整後各歸其所：

| 原住戶 | 新家 |
|--------|------|
| knowledge_base・kb-ext・kb-ss（知識庫三夾） | [docs/kb/](../docs/kb/README.md) |
| html（手工多頁總覽站） | [docs/html/](../docs/html/index.html) |
| core_handy（C++ 原型地基） | [ver_1/try_implement/core_handy/](ver_1/try_implement/core_handy/) |
| try_implement（原型遊樂場） | [ver_1/try_implement/](ver_1/try_implement/) |
| docs（舊架構文檔） | [archive/architecture_docs/](../archive/architecture_docs/) |
| funcs/echo.sh（範例函式） | [ver_1/examples/echo.sh](ver_1/examples/echo.sh) |
