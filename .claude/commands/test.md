---
description: 跑全部測試 — 正式核心 pytest + 兩個原型 smoke test
argument-hint: "[只想跑的範圍，可留空＝全部]"
---
你現在進入 **test 模式**：跑 ai_core 的測試並回報。根目錄 `CLAUDE.md`「建置／測試指令」為權威來源。

## 範圍
$ARGUMENTS

留空＝跑下列全部。

## 指令（環境內已有 `.venv`）
```bash
# 正式核心測試（src/ai_core + tests/）
.venv/bin/python -m pytest -q

# 原型煙霧測試（純標準庫，不需 pytest）
.venv/bin/python try_implement/smoke_test.py
.venv/bin/python try_implement/lib_smoke_test.py
```

## 預期基準（出自 `CLAUDE.md`，若已演進以實跑為準）
- `pytest -q` → 82 passed
- `smoke_test.py` → 72 項斷言
- `lib_smoke_test.py` → 68 項斷言

## 約束
- 若 `.venv` 缺開發相依：`.venv/bin/pip install -e ".[dev]"`。
- **若實際數字與上面基準不符**：依「維護本檔的義務」，提醒使用者（或順手）同步更新 `CLAUDE.md` 的測試數量。
- 全程繁體中文回報：通過數、失敗項與原因摘要。
