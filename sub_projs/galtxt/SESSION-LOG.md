# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **try_1 玩具實驗場：LLM 接口跑通，現行主線＝s7 Lisp**（`sub_projs/galtxt/try_1/`，**刻意不套框架**；細節 [try_1/README](try_1/README.md)）。s7 版 `llm.scm`：`(llm-entry :prompt … :temp …)`，請求 inlet→`s7->json`、`(system "curl…" #t)` 打後端、回應 `json->s7` 導航；離線 curl `file://` E2E 綠。run：`cd try_1; <s7-playground>\s7.exe test.scm`。（C++ 版已放下留作參考。）
  - **✓ 已收（2026-07-14，上輪三待辦全清）**：① 抽出獨立 `json.scm`（llm.scm `(load)` 它）；② **schema 表生成簽章**——`*llm-schema*` 成唯一真相源，`make-llm-entry!` 用 `eval` 生成薄殼 `define*`、`llm-entry-impl` runtime 迭代 schema 塞取樣參數；手寫參數列消滅；三邊界驗過（新參數穿透／未知 keyword 被擋／缺 prompt 報錯）。③ **argv-aware host** `try_1/s7host.c`＋`shim_include/`：綁 `*argv*`（字串 list）＋load 腳本，`wmain`＋`-municode` 中文不亂碼；MinGW 上只需補 `<sys/utsname.h>` 一個 shim（不定義 `WITH_MAIN`、`WITH_C_LOADER` 自動 0）；實測 argv 傳遞／中文／exit code 全綠。build 指令與用法見 [try_1/README](try_1/README.md)「argv host」段。
  - **open 待辦（下一步）**：由 `*llm-schema*` **生成 `--flag` CLI 薄殼**（解析 `s7host.exe` 給的 `*argv*` → 呼叫 `llm-entry`），讓 `s7host.exe cli.scm --prompt "hi" --temp 0.7` 成真——這是「函式參數 ⇄ CLI 旗標由 schema 生成」洞見的最後一哩，schema／host 兩塊地基都已就位。
  - **待使用者**：接真後端（LM Studio 開 server / DeepSeek key）跑一次真回應——見 [WAIT_USER](WAIT_USER.md)。
- **新子專案「舒適 lib＋可 VSCode debug 開發環境」：地基待使用者再定**（與 s7 try_1 **並存、兩邊都推**）。緣由：使用者嫌 s7 語法（想 `'a'` 代 `#\a`、`true` 代 `#t`）＋要成熟 VSCode debug。已比過三案：**s7+reader 糖**（給不了 VSCode debug）、**Racket #lang**（VSCode step-debug 偏弱，使用者否）、**Clojure+Calva**（JVM 太重，使用者否）。使用者要再考慮；已提示更貼合的候選＝**Janet**（原生輕量、原生 `true/false/nil`、內建 debug）與 **Fennel/Lua**（`true/false` 原生、Lua 有成熟 VSCode debug，且 galtxt 技術棧本就列 Lua）。**約束**：須保同像性（schema→CLI meta-programming）。待地基定了才開目錄。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
