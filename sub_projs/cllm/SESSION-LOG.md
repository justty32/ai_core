# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。
> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**。完成即整條刪除。

## 最新進度

- **八語言綁定全路收齊＋一鍵 smoke**（2026-07-16）：全語言（C／C++／Lua／Fennel／s7／Python／CL／Go＋Shell）都綁了 `tools`／`media`／`modalities`＋`on_tool`／`on_media`；每語言各有 `smoke.sh`、[`test/bindings_smoke.sh`](test/bindings_smoke.sh) 一鍵輪跑（9/9 綠）。C++ 另有便利層 `llm.hpp`（聚合 ask／`std::expected`／串流糖／media helpers）＋`llm_reflect.hpp`（`ask_as<T>`／`make_tool<Args>`／`args_as<Args>`／`modality<Config>`）。常駐前綴改 **`~/dev`**（共用目錄、install 冪等覆蓋、勿整個 rm）；s7 原始碼 vendor 進 repo（`bindings/s7/vendor/`），不再依賴 pas。**open 尾巴**：① 未在 Windows 實測；② 未接主 CMake（刻意獨立）；③ 新接口對真後端未實測（記 WAIT_USER）。詳見 [bindings/README](bindings/README.md)。
- **反射生成 CLI 是「參考版」，已拆成清晰結構供重寫參照**：`src/cli.cpp`（orchestrator）＋`cli_flags.{hpp,cpp}`（反射旗標＋`--help`）＋`cli_config.{hpp,cpp}`（config 三層來源＋檔案/mime）＋`cli_internal.hpp`（共用常數）——按關注點拆、各檔 ≤300 行、行為不變（19/19）。**使用者仍將自行重寫一遍（加深記憶）**，這套拆法當骨架。CLI 用法見 [docs/cli-manual.md](docs/cli-manual.md)、檔對應見 [code-map ⑤](workflows/common/code-map/CODE_MAP.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|
| （尚無工作流長出自己的 session-log）| | |

## 不屬任何工作流的進度

- （無）
