# testing — 跑測試（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

## 指令

- **快速驗證（Claude 自己跑、鐵律要求的那套）**：`scripts/run.sh test`（fiveam，目前 8 檢查）
- **完整驗證**：`scripts/run.sh test` 綠燈後，再 `scripts/run.sh build`（確認能編出獨立執行檔 `build/cl-lab`）。
- REPL 內手動跑：`(asdf:test-system :cl-lab)` 或 `(ql:quickload :cl-lab/tests)` 後 `(fiveam:run! 'cl-lab/tests:all-tests)`。

## 測試分類

- 目前測試全走 fiveam、無外部環境依賴，離線機可全跑，不必分類。
- 日後若某些檢查需要實機/外部服務（例如真正的 C 函式庫、網路），在這裡定分類方式，並把 Claude 跑不了的那部分記到 [WAIT_USER](../WAIT_USER.md)。
