# examples/wrapcli — 整合垂直切片（2026-06-28）

把目前**所有非 LLM 設施串成一個真工具**跑一次，驗證它們能協作。是 roadmap「LLM Entry
Manager」的架構雛形——核心暫用確定性的 shell-out（`tr a-z A-Z`）代替 LLM。

## 它串起的設施

| 設施 | 用在哪 |
|---|---|
| `intercept` / `meta_json` | `--metadata` 自我描述（吐九軸 JSON） |
| `cli::resolve` / `is_dry_run` | 解析 `--serve <sock>`、`--dry-run` |
| `io::read_all` / `write_all` | one-shot 模式 batch 讀寫 stdin/stdout |
| `shell::run` | 馴化既有 CLI（spawn `tr`、餵 stdin、收 stdout） |
| `serve::serve_socket` | 長駐成 AF_UNIX server（warm process） |
| `state::StateStore` | 跨請求＋跨程序的請求計數（原子持久） |

## 用法 / 驗證

```bash
wrapcli --metadata            # → 九軸 JSON
echo abc | wrapcli            # → ABC          （one-shot：讀→wrap tr→寫）
echo abc | wrapcli --dry-run  # → abc          （不真執行轉換）
wrapcli --serve /tmp/w.sock   # 長駐；每連線一請求：回 "#<n> <轉換結果>"
```

端到端測試已驗：
- one-shot `hello world` → `HELLO WORLD`；`--dry-run` 原樣回傳。
- server 連 2 次 → `#1 ALPHA` / `#2 BETA`；**重啟 server 後第 3 次 → `#3 GAMMA`**
  （計數續數，證實 StateStore 跨程序持久）。
- g++（`-Wall -Wextra -Wpedantic`）與 CMake 兩鏈零警告。

## 已知簡化（切片從簡，非缺陷）

- `transform` 只取 `shell::run` 的 stdout，忽略 exit code / stderr（真 wrapper 會處理）。
- one-shot 走 batch（`read_all`）；串流 `Reader/Writer` 已另在 main.cpp 驗過。
- 名稱注意：StateStore root 為 `wrapcli/`，勿與同名二進位放同目錄（測試腳本曾因此自刪 binary）。
