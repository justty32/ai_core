# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、環境變數設定、權限操作、需要帳號的下載。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（不留已完成清單，歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔，本檔退回只留導航到各分類檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- **試跑 try_1／try_2 打真 backend**（兩條線共用這關）：Claude 已用 curl `file://` 假回應驗過兩邊整條管線（中文＋取樣參數＋解析全對）；真回應需你這邊備環境——開 LM Studio 載模型＋開 local server（預設埠 1234），或用 DeepSeek key 打雲端。跑通後回報，才好定下一步。
  - **try_1（s7）**：s7 REPL 裡 `(load "llm.scm")` 後 `(llm-entry :prompt "你好" :temp 0.7)`；雲端設 `(set! *llm-base-url* "https://api.deepseek.com/v1")`＋`(set! *llm-api-key* "sk-…")`＋`:model "deepseek-chat"`。步驟見 [try_1/README](try_1/README.md)「s7 版」。
  - **try_2（C++/Lua）**：`cd try_2` 後 `./host.exe cli.lua --prompt "你好" --temp 0.7`（本機 LM Studio 用預設）；雲端 `$env:AI_CORE_LLM_BASE_URL="https://api.deepseek.com/v1"; $env:AI_CORE_LLM_API_KEY="sk-…"` 再 `--model deepseek-chat`。步驟見 [try_2/README](try_2/README.md)。
