# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。
> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**。完成即整條刪除。

## 最新進度

- **常駐開發環境 + 八語言綁定已落地**（2026-07-16）：[`install-dev.sh`](install-dev.sh) 一鍵把 cllm 裝成常駐前綴 `~/repo/dev`（include/lib/bin/pkgconfig，`cmake --install`＋rpath），並搭好 **C／C++／Lua(5.4+5.5)／Fennel／s7／Python／Common Lisp／Go(cgo)** 環境＋Shell CLI，可重現（`rm -rf ~/repo/dev` 重跑）。每個 example 示範 基本 ask／串流／schema→JSON 解析／shell-out CLI；Lisp 家族（s7/CL/Fennel）另有 `image/`（產映像／CLI/lib/執行期修改）。Linux 離線 fixture 全綠。**open 尾巴**：① `tools`／`media`／`modalities` 未綁（只 text/schema/stream 主路）；② 未在 Windows 實測；③ 未接主 CMake（bindings 刻意獨立）。詳見 [bindings/README](bindings/README.md)。
- **反射生成 CLI 是「參考版」，已拆成清晰結構供重寫參照**：`src/cli.cpp`（orchestrator）＋`cli_flags.{hpp,cpp}`（反射旗標＋`--help`）＋`cli_config.{hpp,cpp}`（config 三層來源＋檔案/mime）＋`cli_internal.hpp`（共用常數）——按關注點拆、各檔 ≤300 行、行為不變（19/19）。**使用者仍將自行重寫一遍（加深記憶）**，這套拆法當骨架。CLI 用法見 [docs/cli-manual.md](docs/cli-manual.md)、檔對應見 [code-map ⑤](workflows/common/code-map/CODE_MAP.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|
| （尚無工作流長出自己的 session-log）| | |

## 不屬任何工作流的進度

- （無）
