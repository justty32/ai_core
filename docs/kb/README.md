# kb — 知識庫快照

← [docs](../README.md)

三個知識庫快照夾。**皆為某時點的凍結快照，不在日常維護鏈**——當前事實來源請看 `roadmap.md`（戰略）、`docs/spec/`（規範）、`src/ai_core/`（正式核心）。各夾自帶入口，先讀入口。

| 子夾 | 內容 | 入口 |
|------|------|------|
| [knowledge_base/](knowledge_base/) | **知識庫主快照**：把散在 roadmap/spec/try_implement/docs 的筆記無損壓縮成 `note_`／`doc_`／`code_` 三系列；含 `_build_html.py` 渲染器與整套自包含 HTML 靜態頁（`index.html`＋`kb.css`）。 | [00_INDEX.md](knowledge_base/00_INDEX.md) |
| [kb-ext/](kb-ext/) | **知識庫擴充論述**：對 knowledge_base 的 12 篇擴充建議（`ext_01~12`，把願景操作化為規範）＋專家圓桌 `discussion_logs/`（SSE/SSA/SGA/AIRE 四角色三輪收斂 ATP v0 等）＋ `v0_blueprint/`。 | [00_INDEX.md](kb-ext/00_INDEX.md) |
| [kb-ss/](kb-ss/) | **原始碼／規範快照**：knowledge_base 的純 `.md` 姊妹版（`note_`／`doc_`／`code_`），核心代碼與規範的無損壓縮說明，無 HTML 渲染。 | [00_INDEX.md](kb-ss/00_INDEX.md) |
