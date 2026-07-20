# INDEX — cl-lab 專案地圖

整個專案的頂層導航。cl-lab = **Common Lisp（SBCL）開發環境試驗場：能跑的 ASDF 專案 + 一整套中文教學**。AGENTS.md 只放主工作流 + 指向本檔；細節從這裡分流出去。

---

## Repo 佈局

| 路徑 | 內容 |
|------|------|
| `cl-lab.asd` | ASDF 系統定義（依賴、build 執行檔設定）|
| `src/` | 原始碼：`package.lisp`（套件/命名空間）+ `main.lisp`（核心 + clingon CLI）|
| `tests/` | fiveam 測試（`main.lisp`）|
| `scripts/run.sh` | 統一入口：`run` / `test` / `build` / `repl` / `swank` |
| `examples/` | 可跑範例（ffi / subcommands / conditions / macros）；入口見 [examples/README.md](examples/README.md) |
| `docs/` | 分篇中文教學（00 環境 → 09 C FFI）；入口見 [docs/README.md](docs/README.md) |
| `cl-cheatsheet.html` | 單頁速查表（語言 + 庫濃縮成一頁）|
| `build/` | `asdf:make` 產出的獨立執行檔 `build/cl-lab` |
| [`comfy/`](comfy/README.md) | **舒適 CL 地基**（2026-07-20 併入）：一層順手糖（`true`/`false`、`'a'` 字元、C 風格字串轉義）＋成熟可 VSCode/Alive debug 的 SBCL 環境。框架外實驗場、獨立一攤，自帶入口。 |
| `workflows/` | 工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）|
| `inbox/` | agent 之間的**信件**收件匣（放信處，保持乾淨；使用方式見 [workflows/inbox/](workflows/inbox/README.md)）|

## 工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)** 的派發表——它由你導入時選的 **flavor 包**（開發 / 知識工作）提供並貼入。每個工作流的 durable 知識歸在 `workflows/<該工作流>/` 或單檔 `workflows/<該工作流>.md`（含 `archive/` 封存過時文檔），具體流程在各自入口檔。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**（結構整理原則 + 四級成長軌跡）——**只在要重構/整理結構時取用**。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)；碰原始碼的**程式碼慣例 + 導航 index 維護鏈**在 `common/conventions`（由開發 flavor 包提供）。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[gotchas](workflows/common/gotchas.md) 踩坑（kernel 內建）+ `conventions` 程式碼慣例（開發 flavor 提供）+ `writing` 寫作風格（知識 flavor 提供）|

## 活狀態（只列還沒完成的）

三軸：進度＝我手上的、待使用者＝卡在人、信件＝agent 之間收發（像 email）。

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根）→ 各工作流 session-log（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證的入口（repo 根；膨脹後拆 `wait_todo/` 分類檔）|
| `inbox/`（放信處）+ [workflows/inbox/](workflows/inbox/README.md)（使用方式）| agent 之間的**信件**（可選；像 email，狀態靠位置：inbox 頂層＝未處理、`done/`＝已處理）|
