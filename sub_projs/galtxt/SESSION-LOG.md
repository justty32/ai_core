# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **try_1 玩具實驗場：LLM 接口跑通，現行主線＝s7 Lisp**（`sub_projs/galtxt/try_1/`，**刻意不套框架**；細節 [try_1/README](try_1/README.md)）。同日兩版：
  - **① C++ 版（已放下）**：`llm_entry.cpp`＋vendored simdjson＋curl shell-out，mingw64 編過、HttpListener E2E 綠。使用者定調「**放下 C++，效能瓶頸再說**」，留作參考。
  - **② s7 Scheme 版（現行）**：`llm.scm` 的 REPL 函式 `(llm-entry :prompt … :temp …)`；請求用 inlet→`s7->json`、`(system "curl…" #t)` 打後端、回應 `json->s7` 導航取值；離線 curl `file://` E2E 綠（中文＋取樣參數＋解析全對）。run：`cd try_1; <s7-playground>\s7.exe test.scm`。
  - **關鍵決策/洞見**：悟到「函式參數 ⇄ CLI 旗標」的對應**該由 schema meta-programming 生成、不手寫**（Lisp 同像性正是那台機器）；轉向 s7（runtime＝`pas/derived/s7-playground/s7.exe`）。
  - **open 待辦**：把 `define*` 簽章改由 **schema 表生成**（消滅手寫參數列）；抽出獨立 `json.scm`；**s7 無 argv** → shell `--flag` CLI 待做「argv-aware s7 host（讓 shebang 成真，~15 行 C 綁 `*argv*`＋load）」。
  - **待使用者**：接真後端（LM Studio 開 server / DeepSeek key）跑一次真回應——見 [WAIT_USER](WAIT_USER.md)。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
