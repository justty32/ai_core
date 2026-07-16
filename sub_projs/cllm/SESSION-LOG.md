# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。
> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**。完成即整條刪除。

## 最新進度

- **計畫：cllm 底下開兩個 binding sub_proj（lua／s7 Scheme）**（2026-07-16 定，使用者 clear 後開建）：把 `libcllm.so` 的 C ABI（`cabi.h`／唯一入口 `llm_ask`）綁進 **Lua** 與 **s7（Scheme）** 兩個腳本 VM。預期**小、不需另建開發環境**——消費既有**穩定 C ABI**、連 `libcllm` 即可，不新增 toolchain。結構／落點（`bindings/{lua,s7}/` 之類）與 CMake 掛法待開建時定；C ABI 參考見 [docs/c-abi-reference.md](docs/c-abi-reference.md)。
- **反射生成 CLI 目前是「參考版」**：`src/cli.{hpp,cpp}` 的反射旗標 CLI 是抽離前落地的參考實作，**使用者將自行重寫一遍（加深記憶）**——重寫定案後此條再更新。CLI 用法見 [docs/cli-manual.md](docs/cli-manual.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|
| （尚無工作流長出自己的 session-log）| | |

## 不屬任何工作流的進度

- （無）
