# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——里程碑史交給 git log。待**使用者**親自驗證／追認的另見 [WAIT_USER.md](WAIT_USER.md)。

> 條目格式：每條只留**一行 open 狀態 + 指向細節的連結**，完成即整條刪除。

## 最新進度（open）

- **大檔拆分待進行（下一步重點）** — 使用者要求所有活層 `.md` 降到 **<10KB**。超標清單（10 檔，全在 `workflows/`）：spec 家族 `spec/lib_spec.md`(32K) / `spec/execution_forms.md`(16K) / `spec/axis_spec.md`(15K) / `spec/composite_spec.md`(12K)；研究 `ideas/research/paper_collision_2026-06-27.md`(27K) 與三篇 `paper_collision_round2_*`(21–25K)；`ideas/crystallization_engine_blueprint.md`(22K)；`roadmap.md`(23K)。**方法**：每個大檔按內在結構（spec 章節／roadmap §／里程碑／research 火花條目）升級成資料夾＝子檔＋index，上層只剩導航（見 [DEV-GUIDE.md](DEV-GUIDE.md)「結構整理原則」）。**已定調（採 a，2026-07-13）**：硬性一律 <10KB 全拆——連貫的 research 火花表、roadmap 也照拆，**無「單體可超標」豁免**；並同步把 [DEV-GUIDE.md](DEV-GUIDE.md) 門檻由 8192B/300 行改為**統一 10KB**、移除「本質不可分單體可超標保留」那條。執行方式：每個大檔各派 subagent、升級成資料夾（子檔＋index），上層只剩導航。拆完務必回填 INDEX／WORKFLOWS／各層 index 連結並跑連結檢查（`git ls-files '*.md'` + 相對連結解析）。
- **最硬未決題：固化引擎手動 vs 自動** — 標「這題優先」，見 [roadmap.md](workflows/roadmap.md) §3.6 / §8。
- **v0 最小垂直切片尚未動工** — 一個真實框架 → 一次聰明模型生資產 → 笨模型 + 行數助手 + retry/guard，見 [roadmap.md](workflows/roadmap.md) §6。
- **第一目標問題構想上已換成 galgame 台詞生成（llm_forge），但 roadmap.md §5 尚未動手術** — 北極星不動、入門切口換材料不換架構；正式改稿暫緩到討論收斂。詳見最新長談 [workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
- **刻意暫緩至 v0：A4（組合軸推導）＋ B 系列（B1 語意欄位 / B2 共用模組 / B3 沙箱）** — 留給 v0 切片逼出優先序，見 [roadmap.md](workflows/roadmap.md) §7。
- **workflows/spec/overview.md 仍是「待填」stub** — 規範家族缺總覽定位（這套設計模式是工具集？設計模式？執行組織框架？），見 [workflows/spec/overview.md](workflows/spec/overview.md)「待填：本質是什麼」節。
- **llm_forge 子專案骨架已建**（`sub_projs/llm_forge/`，套 workflows 模板），實質規劃待使用者討論收斂後展開。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

## 不屬任何工作流的進度

- （無）
