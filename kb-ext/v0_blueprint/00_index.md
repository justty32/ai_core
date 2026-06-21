# v0 藍圖 — 圓桌定案成果集

> **這是什麼**：2026-06 五場四角色專家圓桌（SSE/SSA/SGA/AIRE + 主持人收斂）的**定案結論**，提煉成快速查閱層。每檔一主題、保持精簡。
> **完整辯論過程**在 `kb-ext/discussion_logs/`（每主題 `*_opinions.md` + `*_synthesis.md`）；本目錄只留「定了什麼、為什麼」，不留辯論細節。
> **狀態**：全部為設計收斂，`src/ai_core/_core.py` 與 `core_nature/` 尚未改動（除 ATP v0 已登記進 roadmap §6.1 + DECISIONS）。

## 五個主題如何咬合

```
roadmap §6 v0 切片
   │
   ├─[01] ATP v0 ──── 程式碼填碼切片（asset JSON 從 idea.py 流到 sfc.py）
   │                   ↑ 提供底層檔：skeleton/isolation/line_assistant/asset/trace
   ├─[02] stdlib ───── 基礎函數＝確定性原子 / LLM 原子 / ★組合子（這三層是一切的積木）
   │                   ↑ pipe/guard/retry/route/decompose
   ├─[03] coding agent ─ ＝確定性組合子樹（用 02 的組合子 + 01 的模組拼成），非 ReAct
   ├─[04] judge ────── 事實判斷函數＝classify 特例（02 的一個應用範例）
   └─[05] judge 訓練 ── 把 04「凹」到零錯誤＝固化引擎 + 第九軸證書 + 飛輪 的最小實例
```

共同底層：全純標準庫（`ast`/`json`/`subprocess`/`uuid`/`os`），不破壞 `dependencies=[]`。三套（01/03/05）共用同一批底層檔，開工時可一起做。

## 目錄

| 檔 | 主題 | 一句話 | 過程出處 |
|---|------|--------|---------|
| [01_atp_v0.md](01_atp_v0.md) | ATP v0 填碼切片 | append-only asset JSON，逐站追加不改寫上游 | judge 前的 round_2/3 + ratification |
| [02_stdlib.md](02_stdlib.md) | 標準函式集 | 原子 + 組合子的代數，不是 CLI 工具箱 | stdlib_round_1 |
| [03_coding_agent.md](03_coding_agent.md) | coding agent | 確定性組合子樹，LLM 只在葉子填洞/貼標籤 | agent_round_1 |
| [04_judge.md](04_judge.md) | 事實判斷函數 | judge = classify 特例（labels=yes/no/unknown）| judge_round_1 |
| [05_judge_training.md](05_judge_training.md) | 零錯誤函數訓練 | 三層三明治；零錯誤＝證書 stability=100% on test_set | judge_round_2 |

## 跨主題的共同信念（五場反覆出現）

1. **LLM 退縮**：預設一個函數是確定性的；要用 LLM 必須證明確定性程式碼辦不到（真正需要 LLM 的只有語意轉換）。
2. **guard/retry ＝外接理智**：確定性驗證器（ast.parse/json.loads/規則）把笨模型的「偶爾對」變「保證對或明確失敗」。
3. **控制流不交給 LLM**：終止由確定性謂詞持有，「我們沒把方向盤交給乘客」。
4. **拒絕為預設 + 憑證准入**：危險能力預設不在笨模型搆得到的桌面上；第九軸 `nondeterministic`（true＝未認證 → dict＝證書，可撤照）。
5. **飛輪**：聰明模型一次性把隨機性固化成資產，資產降階成確定原子，笨模型免費消費。
