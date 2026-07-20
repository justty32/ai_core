# handy — 重建中（rebuild on cllm/core-py）

handy = **路徑一（把 OS 當 AI agent）的落地試驗田**——一組小腳本/小程式，拿現成程式用腳本包裝、資料夾＝callable、按慣例組合。北極星＝**整個作業系統變成一個 AI agent**。

## 現況（2026-07-20）

專案**淨空重來**：以 [cllm/core-py](../cllm/core-py/README.md)（cllm 的純 Python 平行實作，可 `import cllm` 直接 `ask()`）為新地基重構。

- **舊實作全部凍結於 [archive/](archive/)**——原住戶（`llme`/`zhtw`/`wf`/`mail`/`hermy`，多為 Fennel/shell）、runtime（`inbox/`/`workflows/`）、舊方法論文檔（`AGENTS.md`/`INDEX.md`/`DEV-GUIDE.md` 等）都在裡面，歷史保留、可還原。要看以前長怎樣、設計脈絡、住戶清單 → 進 `archive/`。
- **新結構**：重建開始後在此重寫入口（AGENTS/INDEX/住戶）。

## 為何轉 core-py

舊住戶靠 C++ `llm` CLI（單發、餵不回 tool-role）或各自手搓 HTTP；`core-py` 提供可 import 的 `ask()`（tools/media/stream/schema 皆 kwargs），能把「腦」直接接進 Python 編排器，去掉 shell-out 與 jq 拼裝那層。
