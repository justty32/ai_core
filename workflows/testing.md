# testing — 跑測試 / 驗證（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

跑本 repo 全部測試並回報。鐵律要求「改動後跑驗證確認行為不變」，本檔就是那套指令。對應 slash command `/test`。

## 環境

repo 內已有 `.venv`（Python 3.14）。安裝開發相依：

```bash
.venv/bin/pip install -e ".[dev]"
```

## 指令

- **正式核心測試**（`src/ai_core/` + `tests/`，需 pytest）：

  ```bash
  .venv/bin/python -m pytest -q                 # 目前 101 passed
  ```

- **原型煙霧測試**（不需 pytest，純標準庫）：

  ```bash
  .venv/bin/python try_implement/smoke_test.py      # 83 項斷言
  .venv/bin/python try_implement/lib_smoke_test.py  # 78 項斷言
  ```

「跑全部測試」＝上面三條都跑並回報。

## 跑原型工具（範例）

```bash
.venv/bin/python try_implement/tools/hub.py --metadata
.venv/bin/python try_implement/tools/chain.py ...
```

## 備註

- 目標平台為 POSIX；上列 `.venv/bin/...` 路徑為 POSIX 佈局。
- 測試數量會隨扶正／新功能變動——數字變了記得回填本檔（鐵律 5）。
