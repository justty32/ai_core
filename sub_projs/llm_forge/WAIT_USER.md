# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、環境變數設定、權限操作、需要帳號的下載。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（不留已完成清單，歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔，本檔退回只留導航到各分類檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

源自 idea 筆記 §9.6〔待使用者確認〕的兩個結構決定：

- **結構決定①（升/降主線）**：確認 `core_handy`（C++）**升主線**、`src/ai_core/_core.py`（Python）**降凍結參考**？此決定牽動哪套當地基往下做。
- **結構決定②（使用者自審地基）**：使用者將**自行審查 core_handy 是否合意的地基**。審查清單（都在 `../../try_implement/core_handy/` 底下）：`examples/llm_entry.cpp`、`impl/serve.hpp`、`impl/llm.hpp`、`impl/rate.hpp`、`impl/http.hpp`、`defs/axes.hpp`、`CMakeLists.txt`。四個要回答的問題（詳見筆記 §9.6）：
  1. socket 字串協定是不是想要的那道「縫」？
  2. header-only C++20 的相依姿態可接受嗎？
  3. `llm.hpp` 的 backend 撐不撐本地模型？
  4. Lua 那塊從零起，OK 嗎？

**落腳位置備註**：galgame forge 建在哪（core_handy 原地 vs 新 repo）——使用者**已傾向新 repo**（port core_handy 過去，§9.5）；本子專案 `sub_projs/llm_forge/` 是**現階段的規劃落腳點**，非最終家。
