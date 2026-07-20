# bindings/shell/cli/ — Shell CLI 薄外殼（誠實版）

← [shell binding](../README.md)｜[cllm 傘](../../../README.md)

這是 cllm 的 **Shell 版 CLI 前端**：一支像 `llm` 的命令列入口，**重用 Shell 既有的 binding**。
程式風格與模組敘事**比照 [`core-py`](../../../core-py/README.md)**。

## ⚠ 先講清楚：這一層本質上是 `llm` 的透傳（passthrough）

Shell 的「binding」＝**直接 shell-out 呼叫 `llm` 二進位**（見 [`../example.sh`](../example.sh)：純
用 `llm` ＋ `jq`）。而 `llm` 這支 C++ 執行檔**已經是**一支完整的 unix filter CLI——argv 掃描、
三層 config、`--stream`／`--schema`／`--tool`／`--image`／`--modality`／`--media-out`、四路輸出、
退出碼 0/1/2/130、SIGINT 取消，**全都在 `llm` 裡**。

所以「一支 Shell CLI 外殼」不可避免地**非常接近 `llm` 的透傳**。本目錄刻意**不過度工程化**：
不在 shell 重造 argv/config/media/output（那只是把 `llm` 已有的邏輯再包一遍），而是做一支
**薄、透明、有清楚文件**的 wrapper。

### 這層到底加了什麼價值（老實說，很少）

| 項目 | 說明 |
|------|------|
| 二進位探測＋人話診斷 | 找不到 `llm` 時給清楚訊息（`EXIT_NO_LLM=127`），而非裸 `command not found`；支援 `LLM_BIN` 覆寫路徑 |
| `--meta` 自我描述 | 報告解析到的 `llm` 路徑＋這次會用的 config 檔（**只探測、不解析**），呼應專案的 meta 主題 |
| 完美透傳 | 用 `exec` 交棒——不吞 stdin、不改 stdout、不動退出碼與 SIGINT，wrapper 全程不插手 |

**除此之外，一次發問的一切都是 `llm` 做的。** 想要權威旗標清單就跑 `llm --help`。

## 進入點

```sh
source ~/dev/cllm/env.sh                 # 把 $DEV/bin（含 llm）放進 PATH
bash cli/llm.sh 用一句話介紹你自己         # ＝直接 exec `llm 用一句話介紹你自己`
bash cli/llm.sh --meta                    # wrapper 自我描述（llm 路徑＋config 來源），不呼叫 llm
bash cli/llm.sh --help                    # 透傳給 `llm --help`（權威旗標清單）
```

固定／連線旗標一律照 `llm` 的語意（⚠ 注意：C++ `llm` 的 `--schema`／`--tool`／`--modality`
收**檔案路徑**，與 `core-py` 刻意分歧的「字面 JSON」不同——本 wrapper 透傳，故照 C++ 語意給路徑）。

## 結構（比照 core-py 的職責切分；各檔 <150 行、單一職責）

| 檔 | 對齊的 C++ 檔 | 職責 |
|----|--------------|------|
| `internal.sh` | `cli_internal.hpp` | 退出碼／env 鍵／`_err` 小工具（葉；含 wrapper 專屬 `EXIT_NO_LLM=127`、`LLM_BIN`）|
| `flags.sh` | `cli_flags.cpp` | wrapper 自我描述 `wrapper_usage`（**不**重列 llm 的反射旗標表——那會過時，導到 `llm --help`）|
| `config.sh` | `cli_config.cpp` | 三層 config 來源**探測與報告**（`--config` ＞ `LLM_CLI_CONFIG` ＞ `~/.config/llm/config.json`；只探測不解析）|
| `dispatch.sh` | `http.cpp` 邊界 | 定位並 `exec` `llm`——**shell binding 的內核邊界**（binding＝shell-out）|
| `llm.sh` | `cli.cpp` | 進入點：攔 wrapper 專屬旗標（`--meta`／`--wrapper-help`），其餘全透傳 |

### 刻意「沒有」的對應（誠實交代退化情形）

core-py 的 `argv.py`／`reqinput.py`／`media.py`／`output.py` 在此**無對應檔**：argv 掃描、
請求組裝、`--image` 三分流、四路輸出 Sink（文字／tool_calls／媒體／錯誤）**全由 `llm` 自己做**。
在 shell 重造它們＝多包一層冗餘，違反「不過度工程化」。

## 離線自測

```sh
source ~/dev/cllm/env.sh
bash cli/smoke.sh          # 走 $CLLM_FIXTURES 的 file:// 假回應，逐條 PASS/FAIL，全綠 exit 0
```

驗：基本 ask、`--stream`、`--schema`→jq、`--tool`、media 輸入（`--image`）、media 輸出
（`--media-out`）、退出碼分流（用法錯→1、連不上→2、缺 prompt→1）、`--meta`。
