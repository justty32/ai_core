# 功能開發（feature-dev）— 工作流入口

← [INDEX](../../INDEX.md)｜[AGENTS.md](../../AGENTS.md)

新增 / 修改 cllm 功能的工作流（加旗標、加 vcpkg 依賴、擴 C ABI、修 bug）。這是本工作流的**入口**：先讀本檔，再往下深入。always-on 鐵律見 [AGENTS.md](../../AGENTS.md)；要整理結構時參考 [DEV-GUIDE](../../DEV-GUIDE.md)（被動）；**程式碼慣例 + 導航 index 維護鏈**見 [common/conventions](../common/conventions.md)；技術全貌見 [README](../../README.md)。

## 流程

```
修改（增量）
  → cmake --build --preset linux-debug（建置綠燈）
  → bash test/cli_smoke.sh（離線黑箱 19/19 綠）
  → 若動了真後端行為（錯誤路徑／schema／reasoning）→ 交使用者對真後端驗（記 WAIT_USER）→ 回報 → 修 → 重複
  → 全數通過後：對齊 README/cabi_internal.hpp（code map）→ commit
```

- **自動驗證是你（Claude）自己跑**的把關（鐵律：改完跑驗證）。快速驗證＝`cmake --build --preset linux-debug && bash test/cli_smoke.sh`。
- **離線 fixture 只投影成功、從不投影失敗**——真後端的行為（含錯誤路徑）Claude 跑不了，一律由使用者對真後端驗，記到 [WAIT_USER](../../WAIT_USER.md)（見 [common/gotchas/backend](../common/gotchas/backend.md)）。
- **動對外 C ABI（`src/cabi.h`）要格外小心**：扁平結構／`llm_ask` 簽章／enum 是穩定介面，改前先 grep 受影響的 CLI／客戶端、同 commit 更新（鐵律 3）。
- 測試迭代期間，README/code map 可暫時落後；**commit 前必須對齊**。
- 跨 session 時在 [SESSION-LOG](../../SESSION-LOG.md) 補一行 open 狀態，下個 session 不會誤判已同步。

## 內容

| 檔案 | 內容 |
|------|------|
| `landed/`（長出來才建）| 已落地功能目錄（時間序）|
| `gotchas.md`（長出來才建）| 本工作流專屬踩坑（共通坑在 [common/gotchas](../common/gotchas/README.md)）|
| `session-log.md`（長出來才建）| 本工作流 open 進度（hub 在 [SESSION-LOG](../../SESSION-LOG.md)）|

> **archive**：過時/被取代的文檔封存進 `feature-dev/archive/`。本入口檔若膨脹，照 [DEV-GUIDE「結構整理原則」](../../DEV-GUIDE.md) 拆。
