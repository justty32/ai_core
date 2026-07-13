# sub_projs — 半獨立子專案／知識庫快照／展示產物

← [INDEX](../INDEX.md)

本夾收**半獨立子專案、知識庫快照與展示產物**：它們各自成一攤、與主線（`roadmap.md` / `core_nature/` / `try_implement/` / `src/`）鬆耦合，放這裡讓頂層清爽。各目錄自帶入口，先讀入口再深入。

| 目錄 | 內容 | 入口 |
|------|------|------|
| [core_handy/](core_handy/) | **C++ 原型地基**：ac_helper（`src/ai_core/_core.py` 的 C++ 對應線），含 server-form LLM daemon（one-shot＋`--serve` socket＋跨呼叫 RateMeter）、九軸 `defs/axes.hpp`、設施碼 `impl/*.hpp`。事實基準 = `defs/`＋`impl/`，`notes/` 為歷史 WHY。 | [notes/00_index.md](core_handy/notes/00_index.md) |
| [knowledge_base/](knowledge_base/) | **知識庫主快照**：把散在 roadmap/core_nature/try_implement/docs 的筆記無損壓縮成 `note_`／`doc_`／`code_` 三系列；含 `_build_html.py` 渲染器與整套自包含 HTML 靜態頁（`index.html`＋`kb.css`）。 | [00_INDEX.md](knowledge_base/00_INDEX.md) |
| [kb-ext/](kb-ext/) | **知識庫擴充論述**：對 knowledge_base 的 12 篇擴充建議（`ext_01~12`，把願景操作化為規範）＋專家圓桌 `discussion_logs/`（SSE/SSA/SGA/AIRE 四角色三輪收斂 ATP v0 等）＋ `v0_blueprint/`。 | [00_INDEX.md](kb-ext/00_INDEX.md) |
| [kb-ss/](kb-ss/) | **原始碼／規範快照**：knowledge_base 的純 `.md` 姊妹版（`note_`／`doc_`／`code_`），核心代碼與規範的無損壓縮說明，無 HTML 渲染。 | [00_INDEX.md](kb-ss/00_INDEX.md) |
| [html/](html/) | **手工多頁總覽站**：dark theme 靜態網站（index／vision／architecture／status／milestones／decisions／target），共用 `_shared.css`。專案狀態的「vibe check」。 | [index.html](html/index.html) |
| [docs/](docs/) | **架構文檔**：`ARCHITECTURE.md`＋切分後的 `architectures/`（`01_overview`～`08_changelog`，多為舊版架構，部分已被 roadmap 取代）。 | [architectures/index.md](docs/architectures/index.md) |
| [funcs/](funcs/) | **範例函式**：`echo.sh`（最簡 `--metadata` 協議 + pipeline 傳檔範例）。 | [echo.sh](funcs/echo.sh) |
