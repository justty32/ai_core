# DEV-GUIDE — 結構整理參考（被動 / 按需取用）

← [INDEX](INDEX.md)

**被動參考**，不貫穿日常——只在要**重構 / 整理結構**（拆大檔、分類雜亂資料夾）時取用。
always-on 鐵律（試驗田、<100 行、先 hack 後規範、未確認不 push、全繁中）在 [AGENTS.md](AGENTS.md)，不在此重複。

## 膨脹即拆 / 雜亂即分類

兩個觸發、同一套方法：

- **觸發 A：單體過大** —— **程式碼單檔 ≥ 100 行 → 必須拆**（unipath 硬規，比通用 300 行嚴格）；文檔膨脹亦按職責拆。
- **觸發 B：資料夾雜亂** —— 不同用途的檔混在一起難導航。

方法：
1. **按職責 / 用途語意分**，不是 1/2/3 等分——每塊單一清楚職責。
2. 放進**專屬子資料夾**或既有領域檔。
3. 子資料夾留 README / INDEX 導航；已有則更新。
4. 原位只留**精簡指標 + 連結**，不塞細節。
5. **同步上層**：改動使 [INDEX](INDEX.md) / [AGENTS](AGENTS.md) / 上層 [sub_projs/README](../README.md) 失準時一併更新。

## 已套用的拆檔範例（供參照）

- `unipath_9p.py`（308 行）→ `up_ninep_codec`（編解碼）/ `up_ninep`（server）/ `up_ninep_ops`（分派）/ `up_ninep_client`（自測）+ 薄入口 `unipath_9p.py`。
- env 邏輯 → `up_world`（常數/助手）+ `up_env`（`Env` 類）；`unipath_env.py` 退成相容轉出。
- 原則：**共用邏輯抽 `up_*` 核心模組，各 step 腳本退成薄入口**（import 核心 + 一個 `__main__`）。

## 四級成長軌跡

單檔（檔名即意義）→ 資料夾＋單 README → 資料夾內拆出獨立 INDEX/session-log → 資料夾內開子工作流。
**需要才長，不預先過度設計**；子項消失就往回併級。
