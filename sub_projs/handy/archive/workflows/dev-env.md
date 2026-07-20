# dev-env — 開發環境

← [INDEX](../INDEX.md)｜[WORKFLOWS](../WORKFLOWS.md)

handy 的「肉」多是薄腳本／薄程式，把 cllm 及其 tool 串起來。本檔收環境細節：技術棧、後端、build、daemon 觸發條件、驗證。

## 重度依賴 cllm

本專案吃現成、別憑記憶重建（現成件清單見 [INDEX「不重造」](../INDEX.md)）。建 cllm 見 [cllm/AGENTS.md](../../cllm/core/AGENTS.md)（CMake＋Ninja＋vcpkg）。`llme`/`zhtw` runtime 只需 `llm`（cllm 的 CLI）＋ `fennel` 在 PATH（上 PATH 法見 [llme/README.md](../llme/README.md)）。

## 技術棧〔決定・2026-07-18〕

**腳本層＝LuaJIT ＋ Fennel**（Fennel 編到 LuaJIT，手寫用 Lisp 手感、同 runtime；少 Python——太重、每腳本冷啟 ~50–150ms）。三層分工看性質選：

- **純膠水**（挑檔／轉發／exec）→ 一般選**編譯 C++／shell**（零 interpreter 啟動）；**但若膠水緊接一次慢呼叫**（如 `llme` 接 LLM，啟動稅埋在 token 延遲裡＝雜訊）→ 免建置的 **Fennel** 更順手。llme 就是後者（Fennel 一個檔、shebang 直接跑）。
- **有邏輯的小工具** → **Fennel/LuaJIT**（冷啟 ~1–5ms，甜蜜區；預編成 `.lua` 免重複編譯）。
- **要免重載重狀態** → 掛 **daemon** 當瘦 client（見下）。
- 呼叫 cllm：cllm 已有 Lua＋Fennel 綁定；LuaJIT 還能 **FFI 直接 call `libcllm.so`**（連綁定都省）。

## 後端

- **預設後端＝`deepseek`**〔決定・2026-07-18〕：使用者關掉本地 LM Studio（省電），handy 各住戶一律走 `deepseek` endpoint（DeepSeek 付費額度，OpenAI 相容，key 由 [auto-inject](../llme/README.md) 帶）。**新住戶預設烤 `deepseek`**。
- **本地後端（目前多離線）**：`local`（`google/gemma-4-e4b`）／`qwen`（`qwen3.5-9b`）走本機 LM Studio（`localhost:1234`）。要跑本地測試就先開 LM Studio，再 `llme local …`。本地測試**一律用 gemma、且只用 `google/gemma-4-e4b`（小・快，不要 31b、不要 qwen）**。endpoint 清單見 [llme/README.md](../llme/README.md)。

## daemon（`--serve`）暫緩，但要記對觸發條件〔洞見・2026-07-18〕

**不是**為了省啟動——LLM 呼叫的載入環境時間 vs token 吐出延遲差幾個數量級，那幾 ms 是雜訊，為此做 daemon 不划算。daemon 的**真正觸發＝需要跨呼叫的共享狀態**：

- `llm_entry` 的 `RateMeter` 跨請求累計；
- LLM 當單一資源循序佇列化；
- 活 agent 保 context。

→「操縱活 agent」就是這類，那才是 daemon 正主。現成骨架＝`llm_entry.cpp` 的 `--serve <sock>`（位置見 [INDEX「不重造」](../INDEX.md)）。

## 驗證

- **驅動住戶跑真的**：`./zhtw <文字>`／`echo … | ./zhtw`、`llme deepseek <prompt>`（需 `DEEPSEEK_API_KEY`）。
- **免真後端冒煙**：`LLME_LLM=echo ./llme/_exec <ep> …` 看轉發參數；`--endpoint file://<絕對路徑>` 指向 cllm `test/fixtures` 假回應跑全鏈。
- 各住戶自己的冒煙清單在其入口（見 [llme/README.md](../llme/README.md)「冒煙自測」）。
