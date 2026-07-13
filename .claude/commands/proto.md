---
description: 原型探索 — 在 sub_projs/ver_1/try_implement/ 先寫出來跑跑看，暴露設計缺口
argument-hint: "[要探索的點子或元件，可留空]"
---
你現在進入 **proto（原型探索）模式**：在 `sub_projs/ver_1/try_implement/` 遊樂場做探索性實作。根目錄 `AGENTS.md`（頂層路由器）最高優先。

## 對象
$ARGUMENTS

## 定位（務必先內化）
- `sub_projs/ver_1/try_implement/` 是「**先寫出來跑跑看**」的遊樂場，用來**暴露設計缺口**。
- 這裡的產物**是提案、不是定案**；**不要動** `sub_projs/ver_1/src/ai_core/_core.py` 與 `workflows/spec/`（那是正式核心，要動走 `/spec` 扶正流程）。
- 設計哲學照舊是硬約束：**KISS／輕量／不重造輪子／相依最少**；**只用 Python 3.11+ 標準庫**，無外部相依；POSIX、shell 為一等公民。

## 落點（`sub_projs/ver_1/try_implement/` 既有結構）
- `tools/` — indexer／router／switch／sfc／hub／llm_entry_manager／chain
- `lib/` — 複合規範參考實作＋基礎設施＋llm_call
- `demos/` — 可跑 demo
- 新增工具一律實作 `--metadata`（跨元件硬契約，見 `workflows/common/conventions.md`「核心契約」）。

## 流程
1. 在合適子目錄寫原型；沿用 `ai_core` 真 library 的 `register`/`intercept`（純宣告＋顯式攔截）。
2. 為它補斷言到 `smoke_test.py` 或 `lib_smoke_test.py`，用 `/test` 驗證。
3. 若原型暴露出值得收進規範的缺口 → 記到 `sub_projs/ver_1/try_implement/DECISIONS.md` 當懸案，之後走 `/spec`。
4. 視情況更新 `sub_projs/ver_1/try_implement/README.md`（原型總覽／各檔職責）。

## 約束
- 全程繁體中文；程式碼識別子、CLI 指令保留原文。
