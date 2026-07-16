# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、需要帳號的下載。Claude 能做結構性驗證＋離線煙霧測試到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- （目前無——真後端驗證已於 2026-07-16 完成：`ask_as<T>` required 三欄全吐、tools（C++＋Python）、vision（gemma-4-e4b 認出紅色）、錯誤路徑帶 HTTP 400 原文、真 SSE 串流；modalities 被 LM Studio 靜默忽略，記進 [gotchas/backend](workflows/common/gotchas/backend.md)。）
