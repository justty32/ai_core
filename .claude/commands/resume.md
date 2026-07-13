---
description: 接續上次工作 — 依固定閱讀順序重建上下文並給出下一步
argument-hint: "[本次想聚焦的主題，可留空]"
---
你現在進入 **resume 模式**：快速重建 ai_core 的工作上下文。根目錄 `AGENTS.md`（頂層路由器）最高優先；權威來源為 `SESSION-LOG.md` ＋ `workflows/roadmap.md`（固定閱讀順序已收在 `SESSION-LOG.md`）。

## 本次焦點
$ARGUMENTS

## 固定閱讀順序（出自 `SESSION-LOG.md`「接續工作的固定閱讀順序」）
1. **`workflows/roadmap.md`** — 北極星／戰略主文件，判斷「該不該做、先做哪個」的尺。**一定先讀**。
2. 再依當下焦點擇一：
   - 懸案 → `try_implement/DECISIONS.md`（待定奪的規範決策清單）。
   - 規範現況 → `docs/spec/composite_spec.md`（及 `docs/spec/` 其餘 spec）。
3. **`SESSION-LOG.md`** — 接續上次工作的進度日誌（open 進度＋待使用者事項見 `WAIT_USER.md`）。

## 必須先內化的分層（讀懂本 repo 的關鍵）
| 層 | 位置 | 可信度 |
|---|---|---|
| 北極星／戰略 | `workflows/roadmap.md` | 主文件 |
| 規範 (spec) | `docs/spec/` | 已扶正＝定案；標「待填／待議」＝未定 |
| 原型 (探索) | `try_implement/` | 多為提案，非定案；扶正狀態見其 `DECISIONS.md` |

正式核心程式碼只有一處：`src/ai_core/_core.py`（＋ `tests/test_core.py`）。

## 產出
讀完後用繁體中文回報：(1) 當前專案狀態一句話；(2) 最該推進的下一步（含對應檔案）；(3) 仍待使用者定奪的懸案（若有，引 `DECISIONS.md` 條目）。**先重建上下文再動手改任何東西。**
