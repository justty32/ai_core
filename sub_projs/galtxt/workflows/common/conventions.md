# 程式碼慣例 + 導航 index 維護鏈（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor…）共用這套規矩。純文檔/調查類工作流用不到。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../AGENTS.md)。

> **現況：尚無程式碼**——本檔是骨架，第一片程式碼落地時邊做邊補實況慣例（鐵律：邊執行邊生規範）。

## 程式碼慣例

- **能跑的用 port 別重建**（鐵律 2）——動 LLM 接口那塊先搬 [core_handy](../../../ver_1/try_implement/core_handy/) 的能跑地基，別憑記憶重寫。
- **技術棧**：C++ 效能核心（header-only C++20 姿態，承 core_handy）＋ 內嵌 Lua（膠水／確定性小函式，取代 bash）；少 Python。語言中立的縫＝**LLM socket 協定 ＋ 函式 `--metadata` 文字契約**。
- **單檔行數門檻 300 行**（與 [DEV-GUIDE](../../DEV-GUIDE.md) 觸發 A 一致）：超標觸發檢視、按職責拆。
- **breaking change 前先全域 grep 受影響處、同 commit 更新**；契約（`--metadata` / socket 協定）變動時同步文檔。

## 導航 index（code map）維護鏈

> **現況**：程式碼還沒長出來，尚無 code map。等程式碼多到 agent 找檔困難時再於本區（或獨立 `common/code-map/`）建。以下維護鏈規則屆時生效。

三個面向構成維護鏈：**程式碼 → code map → 文檔**。

**優先級（衝突或時間不夠時，依序保持一致）：** 程式碼 > code map > 文檔。
**code map 與程式碼衝突時：以程式碼為準，立即修正 code map。**

**日常規則：**
1. **修改前**：先讀 code map，找到相關領域，只讀清單中列出的檔案——不要讀無關領域的檔案。
2. **修改後**：若新增或刪除了原始碼檔案，或某檔案的職責有顯著改變，必須同步更新 code map。
3. 原始碼檔案本身**不加**「對應 code map」的註釋（維護成本過高）；反向查找直接 grep code map 文件。
