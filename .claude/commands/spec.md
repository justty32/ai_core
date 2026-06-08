---
description: 規範扶正 — 把 try_implement 提案收斂進 core_nature/ 規範與 _core.py
argument-hint: "[要處理的決策編號或主題，如 A4 / B1，可留空]"
---
你現在進入 **spec（規範扶正）模式**：把原型提案扶正成定案規範。根目錄 `CLAUDE.md` 最高優先。

## 對象
$ARGUMENTS

留空則先讀 `try_implement/DECISIONS.md` 挑出最該處理的懸案，與使用者確認再動手。

## 心智模型（務必先內化）
- `try_implement/` 的東西**即使能跑也只是提案**，不等於規範定案。
- 「扶正」＝把某提案落地進**正式核心**：`src/ai_core/_core.py`（API／九軸）與 `core_nature/`（spec 文件），並補 `tests/test_core.py`。
- 設計哲學是硬約束：**KISS／輕量／不重造輪子／相依最少**；實作**只用 Python 3.11+ 標準庫**，`pyproject.toml` 維持 `dependencies = []`；目標平台 POSIX。

## 流程
1. 讀 `DECISIONS.md` 對應條目（現況／建議／待決點）與相關 `core_nature/*.md`。
2. **拒絕為預設**：除非該環節通過「必要性＋穩定性」兩道閘門，否則不擴張規範（呼應第九軸治理原則）。先選目標問題、讓它告訴你該定哪條——**目標問題＝停止鍵**。
3. 扶正落地：改 `_core.py` ／ 寫 `core_nature/` ／ 補 `tests/test_core.py`，跑 `/test` 驗證全綠。
4. **更新 `DECISIONS.md`**：把該條從「待扶正」移到「✅ 已收斂」，標日期與落地處。
5. **同步 `CLAUDE.md`**（維護義務）：若動到核心 API／軸定義／目錄結構／測試數量／扶正狀態，一併更新。

## 約束
- 未經使用者拍板的開放方向題（B 系列等）依 `roadmap.md §7` **可暫緩至 v0**，不要擅自擴張。
- 全程繁體中文；程式碼識別子、CLI 指令保留原文。
