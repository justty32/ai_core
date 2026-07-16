# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。
> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**。完成即整條刪除。

## 最新進度

- **bindings v1 已落地（Lua＋s7）**（2026-07-16）：`bindings/{lua,s7}/` 綁 C ABI、Linux 離線 fixture 實測綠（非串流／串流 on_delta／schema／錯誤路徑）。**open 尾巴**：① `tools`／`media`／`modalities` 未綁（只 text/schema/stream 主路）；② 未在 Windows 實測（`build.sh` 是 bash）；③ 未接 CMake（刻意）。詳見 [bindings/README](bindings/README.md)「範圍與注意」。
- **反射生成 CLI 目前是「參考版」**：`src/cli.{hpp,cpp}` 的反射旗標 CLI 是抽離前落地的參考實作，**使用者將自行重寫一遍（加深記憶）**——重寫定案後此條再更新。CLI 用法見 [docs/cli-manual.md](docs/cli-manual.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|
| （尚無工作流長出自己的 session-log）| | |

## 不屬任何工作流的進度

- （無）
