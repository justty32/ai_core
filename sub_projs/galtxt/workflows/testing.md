# testing — 跑測試 / 驗證（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

> 本檔是「單檔工作流」，膨脹了就照 [DEV-GUIDE](../DEV-GUIDE.md) 升級成資料夾型。

## 指令

- **快速驗證（Claude 自己跑、鐵律要求的那套）**：
  - `cd gen_v0 && lua main.lua`——示範全數自帶 assert（含確定性逐位元自檢），跑完印「全部示範完成」即綠。
  - `cd gen_v1 && lua main.lua`——事實庫九示範全數自帶 assert，跑完印「九個示範全部通過」即綠。
  - `cd gen_v1_f && fennel main.fnl`——同上的 Fennel 版（Fennel 1.6+）。
- **完整驗證**：全部都跑。

## 測試分類

有「部分測試需要特殊環境」（本機 LLM daemon、外部模型服務、實機）時在此寫清楚分類與各環境能跑哪些——這是「離線也能開發」的關鍵。跑不了的環境依賴驗證 → 記 [WAIT_USER](../WAIT_USER.md)。

- （待補）
