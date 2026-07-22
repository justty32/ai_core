# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證**才能繼續的事——例如：實機/實環境測試、外部服務登入、需要帳號的下載。Claude 能做結構性驗證＋離線煙霧測試到極限；跨不過去的那一關記這裡等使用者。

**只列還沒做的**——做完即移除（歷史看 git log）。

> **膨脹就拆**：待使用者項堆多了，就開 **`wait_todo/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

## 待使用者項

- **`--usage` 雲端真後端驗證**（LM Studio 已於 2026-07-22 驗綠：非串流／串流兩路 usage 正確、`stream_options` 未被拒）：剩 OpenRouter／DeepSeek（需 API key）各打一次 `llm 你好 --usage` 與 `llm 你好 --usage --stream`，確認 ① usage 正常印；② 串流末塊有沒有真的附 usage；③ 有沒有後端不認 `stream_options` 直接 400（若有 → 記 [gotchas/backend](workflows/common/gotchas/backend.md)，並考慮該後端串流別開 `--usage`）。結果回寫 [docs/c-abi-output.md](docs/c-abi-output.md)。

- **llm-login 真 OAuth 往返驗證**（C++ 版）：[tools/llm-login/](tools/llm-login/README.md) 已 C++ 重寫、離線測 24 條＋免瀏覽器整合測全綠（PKCE／URL 組裝／token store／config patch／接 callback→換 token→存檔→patch）。跨不過去的那關要**真帳號＋開瀏覽器**：**最快從 OpenRouter 驗**（免註冊 client）——先建 `cmake --build --preset linux-debug --target llm-login` → `cp tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json` → `./build/tools/llm-login login`（headless 加 `--no-browser`，另開瀏覽器貼網址）→ 確認拿到 key、寫進 `config.json`、`llm` 裸跑帶得上 bearer。其餘三家要先各自備好（Google OAuth Client／Entra app＋tenant／GitHub OAuth App）再驗。踩到的真實行為（scope、token 壽命、refresh 是否輪換、redirect_uri 白名單、OpenRouter 回應欄位是否真為 `key`）回寫 preset `_notes`／README／gotchas。

- **anthropic-proxy 真 Anthropic 往返驗證**（C++ 版）：[tools/anthropic-proxy/](tools/anthropic-proxy/README.md) 已 C++ 重寫、端對端假上游驗過（非串流／串流／[DONE]／缺 key→401／x-api-key＋version 帶對）。跨不過去的那關要**真 `sk-ant-` key**：`cmake --build --preset linux-debug --target anthropic-proxy` → 起 `./build/tools/anthropic-proxy`（或 `tools/anthropic-proxy/service/proxyctl.sh start`）→ `cp tools/anthropic-proxy/configs/opus.json ~/.config/llm/config.json`＋填 key → `llm 哈囉`／`llm --stream 短詩`。踩到的真實行為（Anthropic 是否收 `max_tokens` 整數浮點、串流事件邊界、錯誤外殼欄位、模型 id 現行值）回寫 README／models.json。

- （前一項留存：真後端驗證已於 2026-07-16 完成：`ask_as<T>` required 三欄全吐、tools（C++＋Python）、vision（gemma-4-e4b 認出紅色）、錯誤路徑帶 HTTP 400 原文、真 SSE 串流；modalities 被 LM Studio 靜默忽略，記進 [gotchas/backend](workflows/common/gotchas/backend.md)。）
