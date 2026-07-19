# 程式碼慣例 + 導航 index 維護鏈（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor / specs / plans…）共用這套規矩。純文檔/調查類工作流用不到。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../AGENTS.md)。

## 程式碼慣例

- **一切改動先讓 `scripts/run.sh test` 綠燈**；改到 build/entry 相關再跑一次 `scripts/run.sh build`。
- **套件（package）在 `src/package.lisp` 集中定義**；新增對外符號要記得 `:export`，否則其他檔／測試看不到。
- **依賴改在 `cl-lab.asd` 的 `:depends-on`**，然後 `(ql:quickload :cl-lab)` 讓 Quicklisp 抓；新增原始碼檔要同步加進 `.asd` 的 `:components`（`:serial t` 依序載入，順序有意義）。
- **單檔超過 300 行**即觸發檢視、考慮按領域拆（見 [DEV-GUIDE](../../DEV-GUIDE.md) 觸發 A）。目前 `src/main.lisp` 一支示範 CLI/JSON/資料結構，膨脹了再拆。
- **教學文檔（docs / examples / cheatsheet）與程式碼是一體**：改了會影響教學語義的行為，回頭同步對應的 `docs/*.md` 或範例；判斷不用改就在變更說明裡明講「文檔無需更新」。
- Lisp 慣例：kebab-case 命名、`defun`/`defmacro`/`defvar`(`*earmuffs*`)，述詞以 `-p` 結尾。

## 導航 index（code map）維護鏈

> 本專案程式碼還很小（src 兩檔 + tests 一檔），[INDEX.md](../../INDEX.md) 的「Repo 佈局」表就兼任 code map，尚不需獨立 code-map 檔。等 `src/` 長到 agent 找檔困難，再按領域拆出獨立導航 index。

三個面向構成維護鏈：**程式碼 → code map → 文檔**。

**優先級（衝突或時間不夠時，依序保持一致）：** 程式碼 > code map > 文檔。
**code map 與程式碼衝突時：以程式碼為準，立即修正 code map。**

**日常規則：**
1. **修改前**：先讀 code map，找到相關領域，只讀清單中列出的檔案——不要讀無關領域的檔案。
2. **修改後**：若新增或刪除了原始碼檔案，或某檔案的職責有顯著改變，必須同步更新 code map。
3. 原始碼檔案本身**不加**「對應 code map」的註釋（維護成本過高）；反向查找直接 grep code map 文件。
