# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、需要帳號的下載。Claude 能做結構性驗證＋離線煙霧測試到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- **`archived/` 的處置待決（14 個舊 L0 檔目前「工作樹已刪、未提交」）**：使用者 2026-07-16 指示「archive 不要 commit」，故 fork 刪的那批一路排除在所有 commit 外——`git status` 會顯示 14 個 `D sub_projs/cllm/archived/*` 未暫存刪除，這是**刻意暫留、非遺漏**。⚠ **別自作主張 commit 或還原**：三選一（正式提交刪除／`git checkout -- sub_projs/cllm/archived` 還原／維持現狀）由使用者定。
- **常駐 dev 環境是否記進 `~/code/notes`**：使用者說回家先看過再決定，暫不記（那是另一個 repo、push 紀律「說了才推」）。
- **真後端行為驗證需本機 LM Studio（或雲端 API）**：離線 `file://` fixture 只投影「成功」、從不投影「失敗」，錯誤路徑／reasoning 模型的 `max_tokens` 行為／schema `required` 等只有打真後端才驗得出（見 [gotchas/backend](workflows/common/gotchas/backend.md)）。要驗證新接口對真後端的行為，需使用者這邊掛好後端、把 `--endpoint` 指過去實跑。
