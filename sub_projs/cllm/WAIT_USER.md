# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、需要帳號的下載。Claude 能做結構性驗證＋離線煙霧測試到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- **llm-login 真 OAuth 往返驗證**：[tools/llm-login/](tools/llm-login/README.md) 骨架已落地、離線煙霧全綠（PKCE 正確性／URL 組裝／token store round-trip／config patch）。跨不過去的那關要**真供應商＋真帳號＋開瀏覽器**：① 挑一家**有提供程式化 OAuth**的供應商（②類，非消費訂閱），把 `oauth.example.json` 填成真值放 `~/.config/llm/oauth.json`；② 跑 `python3 llm_login.py login` 完成瀏覽器授權，確認 token 寫進 `config.json` 的 `api_key`、`llm` 裸跑帶得上 bearer；③ 驗 `refresh`／`token` 自動續期。踩到的真實行為（token 壽命、refresh 是否輪換、redirect_uri 白名單要求）回寫 README／gotchas。

- （前一項留存：真後端驗證已於 2026-07-16 完成：`ask_as<T>` required 三欄全吐、tools（C++＋Python）、vision（gemma-4-e4b 認出紅色）、錯誤路徑帶 HTTP 400 原文、真 SSE 串流；modalities 被 LM Studio 靜默忽略，記進 [gotchas/backend](workflows/common/gotchas/backend.md)。）
