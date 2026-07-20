# inbox 協議 — agent 之間的非同步信件交接（可執行版）

← [INDEX](../INDEX.md)｜[WORKFLOWS](../WORKFLOWS.md)

workflows repo `template/workflows/inbox`（email 式交接）的**可執行復刻**。用途：**太複雜、不想當場等**的任務——寫成一封信投進 `inbox/`，之後由更成熟的 agent（claude code）非同步 drain 掉。

**它像 email、不像聊天**：寄出就好，慢一點或沒馬上處理都無妨；沒有送達保證、沒有 ack。**狀態靠位置**：`inbox/` 頂層＝待處理、`inbox/done/`＝已處理。

## 工具＝`mail`，信箱＝`inbox/`

（工具不叫 `inbox` 是因信箱資料夾已佔用 `inbox/` 這個路徑。）

| 命令 | 做什麼 |
|------|--------|
| `mail send <任務...>` | 寫一封信 `inbox/<slug>.md`（`… \| mail send <任務>` 可帶上下文），立即返回 |
| `mail list` | 列出待處理的信（`done/` 不列）|
| `mail run [slug\|--dry]` | 逐封 spawn `claude -p` 處理，**成功才 mv 到 `done/`**（失敗保留在 inbox）|

也可從 `wf` 投遞：**`wf -i <任務>`** ＝ 轉呼 `mail send`（wf 的第三模式，非同步）。

## 語意細節

- **slug**＝任務前段轉 kebab、限 16 字（UTF-8 安全）；撞名補 `-2`/`-3`（無鎖，靠改名避撞）。
- **信件格式**：`# <slug>` ＋ `## 任務` ＋ `## 上下文` ＋ 尾註。純 markdown。
- **`mail run` 的處理**：把整封信當 prompt 餵給 `claude -p --permission-mode $WF_PERMISSION`（預設 `acceptEdits`），claude 在**當前工作目錄**動手。退出碼 0 才歸檔；非 0 保留、報錯。
- **信件是 runtime 產物、不進版控**（`inbox/.gitignore` 擋 `*.md`，只留 `.gitkeep` 骨架）。

## 之後可長的

- **daemon 常駐 drain**：現在 `mail run` 是手動觸發；未來 daemon 住戶可常駐輪詢 `inbox/` 自動 drain（見 [dev-env daemon 觸發條件](dev-env.md)）。
- **多 agent 通訊錄 / 回信**：目前單信箱、無回信地址；要跨資料夾交接再補（照 workflows repo `inbox` 的 CONTACTS / 回信地址）。
