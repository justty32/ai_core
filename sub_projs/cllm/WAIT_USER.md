# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、需要帳號的下載。Claude 能做結構性驗證＋離線煙霧測試到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- **真後端行為驗證需本機 LM Studio（或雲端 API）**：離線 `file://` fixture 只投影「成功」、從不投影「失敗」，錯誤路徑／reasoning 模型的 `max_tokens` 行為／schema `required` 等只有打真後端才驗得出（見 [gotchas/backend](workflows/common/gotchas/backend.md)）。要驗證新接口對真後端的行為，需使用者這邊掛好後端、把 `--endpoint` 指過去實跑。
