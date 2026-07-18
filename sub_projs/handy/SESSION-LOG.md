# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——已落地住戶的 durable 知識歸各自入口（`llme/README.md`、`zhtw` 檔頭、[dev-env](workflows/dev-env.md)、[gotchas](workflows/common/gotchas.md)），過程細節交給 git log。待**使用者**親自驗證/做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 handy 頂層新立 `session_logs/` 按住戶/類別拆檔＋一個 index 導航（照 [DEV-GUIDE](DEV-GUIDE.md)）。

## 最新進度（open）

- **〔待動手〕daemon（下一個住戶）**〔note 0718 §六提案〕：常駐小程式，client 寫命令進一個檔／socket，daemon 讀到就 `claude -p` 起 headless run；命令通道複用 append-log。**動手前先確認觸發條件成立**（跨呼叫共享狀態／操縱活 agent）——見 [dev-env「daemon 觸發條件」](workflows/dev-env.md)。現成骨架見 [INDEX「不重造」](INDEX.md)（`llm_entry.cpp --serve`）。

## 各住戶/工作流 session-log

> 某住戶長出自己的 `session-log.md` 後在這裡加一列。一開始空表很正常。

| 住戶/工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何住戶的進度

- （無）
