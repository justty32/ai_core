# docs — 快照家族

← [INDEX](../INDEX.md)

**ai_core 的「快照家族」入口**：知識庫快照與手工總覽站兩攤都收在這，皆為凍結快照性質。薄薄一層導引，細節下沉到各子夾自己的入口。

> **權威設計規範已移出** docs/，改放 [workflows/spec/](../workflows/spec/)（歸入活的規劃層）；docs/ 現在只剩凍結快照。

| 子夾 | 內容 | 入口 |
|------|------|------|
| [kb/](kb/README.md) | **知識庫快照三夾**：knowledge_base（主快照，md+html+builder）／kb-ext（擴充論述＋圓桌）／kb-ss（純 md 快照）。皆為某時點凍結快照，不在日常維護鏈。 | [README.md](kb/README.md) |
| [html/](html/) | **手工多頁總覽站**：dark theme 靜態網站（index／vision／architecture／status／milestones／decisions／target），共用 `_shared.css`。專案狀態的「vibe check」。 | [index.html](html/index.html) |
