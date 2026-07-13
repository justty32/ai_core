# testing — 跑測試 / 驗證（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

> **現況：尚無驗證指令**（galtxt 規劃/起步期、無程式碼）。第一片程式碼落地時在此補上（依技術棧 C++／Lua：如 `cmake --build` / ctest / Lua 測試 runner）。本檔是「單檔工作流」，膨脹了就照 [DEV-GUIDE](../DEV-GUIDE.md) 升級成資料夾型。

## 指令

- **快速驗證（Claude 自己跑、鐵律要求的那套）**：`（待補）`
- **完整驗證**：`（待補）`

## 測試分類

有「部分測試需要特殊環境」（本機 LLM daemon、外部模型服務、實機）時在此寫清楚分類與各環境能跑哪些——這是「離線也能開發」的關鍵。跑不了的環境依賴驗證 → 記 [WAIT_USER](../WAIT_USER.md)。

- （待補）
