# 程式碼慣例 + 導航 index 維護鏈（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor / specs / plans…）共用這套規矩。純文檔/調查類工作流用不到。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../AGENTS.md)。

> **現況**：本子專案處於規劃期、**尚無程式碼**，本檔只先立最小方向性內容；**其餘慣例隨第一片程式碼再長**（別預先立法，呼應「先執行再說、邊執行邊生規範」）。

## 程式碼慣例

- **技術棧方向**（〔§9.2 決定/方向〕）：主力 **C++ 效能核心 ＋ 內嵌 Lua VM**（Fennel 編譯成 Lua ⇒ 使用者手寫用 Lisp/Fennel、同 runtime）；**少 Python、少 bash**。
- **語言中立的縫**（〔§9.2–9.3〕）：跨語言只靠兩道文字契約——**LLM 的 socket 協定**（瘦客戶端「開 socket、寫 prompt、讀回覆」即可，不必每語言寫 SDK）＋函式的 **`--metadata` 文字契約**（每支小函式自述 entries/network/uncertainty…）。
  - 協定岔路〔§9.3 懸，先不選〕：Unix socket 字串協定（現況、KISS） vs 本機 HTTP/OpenAI 相容 gateway（通用、較重）。**先用能跑的字串版**，等消費者逼著長。
- **其餘慣例（拆檔方式、單檔行數門檻、breaking change 前 grep + 同 commit 更新、schema/型別同步…）隨第一片程式碼再長**。

## 導航 index（code map）維護鏈

> **現況**：尚無程式碼、無 code map。等程式碼長到 agent 找檔困難時再建（可獨立成 `common/code-map/`）。下方維護鏈規則保留備用。

三個面向構成維護鏈：**程式碼 → code map → 文檔**。

**優先級（衝突或時間不夠時，依序保持一致）：** 程式碼 > code map > 文檔。
**code map 與程式碼衝突時：以程式碼為準，立即修正 code map。**

**日常規則：**
1. **修改前**：先讀 code map，找到相關領域，只讀清單中列出的檔案——不要讀無關領域的檔案。
2. **修改後**：若新增或刪除了原始碼檔案，或某檔案的職責有顯著改變，必須同步更新 code map。
3. 原始碼檔案本身**不加**「對應 code map」的註釋（維護成本過高）；反向查找直接 grep code map 文件。
