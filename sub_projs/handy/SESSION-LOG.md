# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——已落地住戶的 durable 知識歸各自入口（`llme/README.md`、`zhtw` 檔頭、[dev-env](workflows/dev-env.md)、[gotchas](workflows/common/gotchas.md)），過程細節交給 git log。待**使用者**親自驗證/做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 handy 頂層新立 `session_logs/` 按住戶/類別拆檔＋一個 index 導航（照 [DEV-GUIDE](DEV-GUIDE.md)）。

## 最新進度（open）

- **〔可用・MVP，待擴充〕`wf`（兩層任務派發器）＋`mail`（inbox 協議）**：
  - `wf`：兩層派發，`-b`/`-a`/`-i` 三模式。**auto 路由＝啟發式關鍵詞（免 LLM）＞ llme 分類 fallback**〔2026-07-18・部分求值〕——清楚任務 0 個路由 call（brain 由 2→1 call、agent 省掉分類 call），只有兩類詞都中/都沒中才花一個 llme 分類。argv/stdin/上下文、繁中 append、啟發式六情境＋真 DeepSeek 端到端皆驗綠。
  - `mail`：inbox 協議可執行版——`send`/`list`/`run`＋信箱 `inbox/`。send/list/撞名/UTF-8 slug/`run --dry`/`run` 歸檔 done/／失敗保留、`wf -i` 投遞皆驗綠（claude 用 echo 模擬）。
  - **未由使用者驗收**：真 agent 動手端到端——`wf -a <改檔任務>` 與 `mail run` 都會 **spawn 真 claude** 用 `acceptEdits` 動手改當前目錄（全自主要 `WF_PERMISSION=bypassPermissions`）。使用者上次擋掉真 spawn，待其驗收。
  - **可長**：daemon 常駐自動 drain inbox、多 agent 通訊錄/回信（見 [workflows/inbox.md](workflows/inbox.md)「之後可長的」）。
- **〔可用・MVP，待長〕`hermy`（hermes-agent 的 handy 化復刻，Python）**：DeepSeek function-calling 迴圈＋技能＝folder-as-callable（`skills/<name>/skill.json`＋`run`）＋`create_skill` 自我擴展＋`memory/log.ndjson`。真 DeepSeek 驗綠：工具呼叫迴圈（shell 技能）、**自我擴展**（當場寫 `c2f` 技能再用、37°C→98.6°F）皆通。**待長**：Curator 自我維護技能、記憶預取/壓縮、技能路由部分求值、`shell` 技能權限守衛。分析源 `~/repo/pas/analysis/hermes-agent/`、原始碼 `~/repo/pas/projects/hermes-agent/`。入口 [hermy/README.md](hermy/README.md)。
- **〔待動手〕daemon（下一個住戶）**〔note 0718 §六提案〕：常駐小程式，client 寫命令進一個檔／socket，daemon 讀到就 `claude -p` 起 headless run；命令通道複用 append-log。**動手前先確認觸發條件成立**（跨呼叫共享狀態／操縱活 agent）——見 [dev-env「daemon 觸發條件」](workflows/dev-env.md)。現成骨架見 [INDEX「不重造」](INDEX.md)（`llm_entry.cpp --serve`）。

## 各住戶/工作流 session-log

> 某住戶長出自己的 `session-log.md` 後在這裡加一列。一開始空表很正常。

| 住戶/工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何住戶的進度

- （無）
