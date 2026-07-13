# testing — 跑測試 / 驗證（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

跑本 repo 全部測試並回報。鐵律要求「改動後跑驗證確認行為不變」，本檔就是那套指令。

> **備註**：測試對象整套已封存進 [`sub_projs/ver_1/`](../sub_projs/ver_1/)（封存版）；testing 現在是對那份封存快照做**回歸驗證**（確認參考版行為未壞），不是主線開發驗證。下列指令均在 `sub_projs/ver_1/` 目錄下跑（`src/` `tests/` `try_implement/` `pyproject.toml` 都在該處）。

## 環境

repo 內已有 `.venv`（Python 3.14）。`pyproject.toml` 現在 `sub_projs/ver_1/` 下，安裝開發相依需在該目錄跑：

```bash
cd sub_projs/ver_1 && ../../.venv/bin/pip install -e ".[dev]"
```

## 指令

（以下均先 `cd sub_projs/ver_1`）

- **正式核心測試**（`sub_projs/ver_1/src/ai_core/` + `sub_projs/ver_1/tests/`，需 pytest）：

  ```bash
  ../../.venv/bin/python -m pytest -q                 # 目前 101 passed
  ```

- **原型煙霧測試**（不需 pytest，純標準庫）：

  ```bash
  ../../.venv/bin/python try_implement/smoke_test.py      # 83 項斷言
  ../../.venv/bin/python try_implement/lib_smoke_test.py  # 78 項斷言
  ```

「跑全部測試」＝上面三條都跑並回報。

## 跑原型工具（範例）

（同樣先 `cd sub_projs/ver_1`）

```bash
../../.venv/bin/python try_implement/tools/hub.py --metadata
../../.venv/bin/python try_implement/tools/chain.py ...
```

## 備註

- 目標平台為 POSIX；上列 `.venv/bin/...` 路徑為 POSIX 佈局。
- 測試數量會隨扶正／新功能變動——數字變了記得回填本檔（鐵律 5）。
