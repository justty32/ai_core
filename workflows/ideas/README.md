# ideas — 點子與研究的家

← [INDEX](../../INDEX.md)｜[WORKFLOWS](../../WORKFLOWS.md)

**ideas/ 是點子與研究的家；東西在這裡成熟後，沿規劃管線一路往外走，最後落到規範與程式碼。**

> 口述線一條龍（raw→cleaned→匯總筆記）已拆成獨立工作流：raw/cleaned 在 [intake/](../intake/README.md)、匯總筆記在 [notes/](../notes/)。本夾現在只留**腦暴產物 brainstorm/**、**研究 research/** 與 **spec 候選厚檔**。

## 規劃管線與 repo 落點

點子不是憑空變成核心，中間有階段。每個階段有它在 repo 裡的家：

| 階段 | 這是什麼 | repo 落點 |
|------|---------|-----------|
| **idea** | 剛捕捉的點子、概念拓展草稿 | 口述線 [intake/](../intake/README.md)（raw→cleaned）→[notes/](../notes/) 匯總筆記；腦暴 [`brainstorm/`](brainstorm/)（本夾）|
| **research** | 外部論文與北極星對撞的火花表 | [`research/`](research/)（本夾）|
| **spec 候選** | 已具體到有函式簽章／流程、但尚未定案的厚檔 | 本夾頂層厚檔（如 crystallization_engine_blueprint.md）|
| **spec 定案** | 經 spec 工作流扶正後的權威規範 | [spec/](../spec/) |
| **plan** | 已排出開工次序與驗收標準的施工計畫 | [plans/](../plans/README.md)（施工規劃）＋ [roadmap.md](../roadmap.md) §6（戰略 v0 最小垂直切片）|
| **build** | 真的寫出來 | 早期實作已封存（半封存），當前無在跑的實作 |

> 管線是**成熟度階梯**，不是硬性流程：點子可以跳級、也可以退回。定案權威永遠在 [spec/](../spec/)，本夾內容多為「彙整／提案層」，非定案規範。

## 本夾結構

| 子項 | 內容 |
|------|------|
| `brainstorm/` | critique（找漏洞）／expand（擴展）產物 |
| [`research/`](research/) | 論文碰撞火花表（paper_reading 論文群 × ai_core 北極星）|
| `crystallization_engine_blueprint.md` | spec 候選厚檔（見下表）|

> 口述線的 `raw/`、`cleaned/` 已移至 [intake/](../intake/README.md)、`notes/` 已移至 [notes/](../notes/)。

頂層散檔（逐列）：

| 檔案 | stage | 說明 |
|------|-------|------|
| [crystallization_engine_blueprint.md](crystallization_engine_blueprint.md) | `spec-candidate` | 固化引擎 v0 工程藍圖（有函式簽章／編排流程／里程碑）；待與 [spec/composite_spec.md](../spec/composite_spec.md) 及 DECISIONS.md A4 合併審議 |
| [research/paper_collision_2026-06-27.md](research/paper_collision_2026-06-27.md) | `research-idea` | 正式彙整版（整合三份 round2 + 跨融合）|
| [research/paper_collision_round2_constrained_codegen.md](research/paper_collision_round2_constrained_codegen.md) | `research-idea` | 第二輪・受約束程式碼生成 |
| [research/paper_collision_round2_program_synthesis.md](research/paper_collision_round2_program_synthesis.md) | `research-idea` | 第二輪・程式合成／歸納 |
| [research/paper_collision_round2_self_improving.md](research/paper_collision_round2_self_improving.md) | `research-idea` | 第二輪・自我改進 agent |

## `stage:` 標記值域

frontmatter 的 `stage:` 一眼標出這份東西在管線的哪個成熟度：

- **`research-idea`** — 論文碰撞產出的火花，尚未收斂成任何規範主張。
- **`spec-candidate`** — 已具體到可審議是否扶正為規範的厚檔（函式簽章／流程／里程碑俱全）。
- **`concept-draft`** — 概念框架草稿，拓展設計面但非定案。
