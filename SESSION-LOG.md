# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——里程碑史交給 git log。待**使用者**親自驗證／追認的另見 [WAIT_USER.md](WAIT_USER.md)。

> 條目格式：每條只留**一行 open 狀態 + 指向細節的連結**，完成即整條刪除。

## 最新進度（open）

- **最硬未決題：固化引擎手動 vs 自動** — 標「這題優先」，見 [roadmap.md](workflows/roadmap/README.md) §3.6 / §8。
- **v0 最小垂直切片尚未動工** — 一個真實框架 → 一次聰明模型生資產 → 笨模型 + 行數助手 + retry/guard，見 [roadmap.md](workflows/roadmap/README.md) §6。
- **第一目標問題構想上已換成 galgame 台詞生成（落地為 galtxt，依賴 llm_forge 框架），但 roadmap.md §5 尚未動手術** — 北極星不動、入門切口換材料不換架構；正式改稿暫緩到討論收斂。詳見最新長談 [workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
- **刻意暫緩至 v0：A4（組合軸推導）＋ B 系列（B1 語意欄位 / B2 共用模組 / B3 沙箱）** — 留給 v0 切片逼出優先序，見 [roadmap.md](workflows/roadmap/README.md) §7。
- **workflows/spec/overview.md 仍是「待填」stub** — 規範家族缺總覽定位（這套設計模式是工具集？設計模式？執行組織框架？），見 [workflows/spec/overview.md](workflows/spec/overview.md)「待填：本質是什麼」節。
- **galtxt/llm_forge 角色切分（新定調 7/13，文檔已對齊）**：`sub_projs/galtxt/`＝**動手實驗場**（先執行、跑通想法、galgame 生成器本身），`sub_projs/llm_forge/`＝**框架/爐子**（確定的機制才從 galtxt 搬來固化）；galtxt 依賴 llm_forge。兩邊 AGENTS/INDEX/WORKFLOWS 均已改述對齊。galtxt 已套 workflows 分層模板建骨架、無程式碼，**第一片切口待定**（port core_handy LLM 地基 vs 4.2 rating 工具），見 [sub_projs/galtxt/SESSION-LOG.md](sub_projs/galtxt/SESSION-LOG.md)。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

## 不屬任何工作流的進度

- （無）
