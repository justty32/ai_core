# SESSION-LOG — handy 進度日誌

← [README](README.md)｜[AGENTS.md](../../AGENTS.md)

**只放「還沒完成」的活狀態**（in-flight / open）；完成即整條刪除、里程碑史交給 git log。
文末「開發環境」節是例外——本機 durable 注記，長期保留。

## 最新進度（open）

- **正在評估「把 handy 核心換成 litellm / openai SDK」（CLI 形狀不動）——「先試試看」階段、未定**（2026-07-21）。起於「pllm 是否重造輪子」的長談，收斂出的判斷：
  - **pllm 判活**：零依賴是真北極星（使用者信到「最終連 LLM 都不依賴」），pllm 作為零依賴 LLM 地基站得住。
  - **「CLI＝跨語言 binding」**：非 Python 消費者不必走 FFI，`exec("llme …")` 即天然跨語言接口，FFI 的優勢在 LLM 場景（進程開銷 vs 秒級推理）幾乎全蒸發。
  - **C++ cllm 判退休（非斬立決）**：效能無意義（client 語言不是瓶頸）＋跨語言可用 CLI 取代。退休成本＝現有**非 Python 消費者從 FFI 遷到 shell-call pllm CLI** 的工作量，待盤點。
  - litellm 方向為使用者主動要試；環境戰證明它在此環境成本高（見下），且 SDK 選擇與「能否連 API」正交。
  - **open 尾巴**：①「llme CLI 形狀不變、底層走 openai SDK」的最小 PoC 未寫（venv 已備妥，見下）；②真連 API 待使用者 DeepSeek key（至今只跑 file:// fixture 離線）；③C++ cllm 非 Python 消費者現況待盤（定退休時程）；④pllm 去留最終取決於 litellm/openai PoC 的體感。

## 開發環境（本機 durable 注記 · Windows）

> ⚠ 本專案定位為 **POSIX**（AGENTS.md 鐵律），以下純屬使用者當前 **Windows 11 開發機**的實測約束，非專案目標平台。

**這台 Windows 跑「連網 python」的可行配置**（2026-07-21 血淚實測）：

- **應用程式控制策略封鎖非簽名 openssl DLL**：uv 自管的 python-build-standalone 版 `libssl-3-x64.dll` 被擋 → `import ssl` 失敗 → **連不了任何 https**（litellm / openai / pllm 真連 API 全掛；只有 file:// 離線 fixture 不受影響）。
- **官方 python.org 簽名版 ssl 可用**（OpenSSL 3.0.15 通過）→ **解釋器必用官方安裝器版，不能用 uv 自管 standalone**。
- **可行鏈路**：官方簽名 python（提供可用 ssl）＋ uv 當 venv/套件管理器（`--python` 指官方版路徑）＋ SDK。實測 `openai 2.46.0` 純 wheel 秒裝、`httpx` TLS client 通。venv 位置：scratchpad `api-poc`（暫存，非持久）。
- **⚠ uv 陷阱**：`uv python find` 預設優先挑自管 standalone（ssl 壞的那份）。解法＝卸掉它（`uv python uninstall`）或每次 `--python` 顯式指官方路徑（`C:/Users/WG-Guanyu/AppData/Local/Programs/Python/Python313/python.exe`）。
- **Python 3.14 太新**：litellm/openai 的 Rust 依賴（pydantic-core / jiter）無 3.14 wheel → **用 3.13**（wheel 生態齊、handy 的 3.11+ 也吃）。
- **litellm 1.93+ 自帶 `litellm-rust`**：需現場編譯（無 cargo 則失敗）；`uv pip install --only-binary :all:` 可退到有純 wheel 的版本繞過。
- **SDK 選型**：若放棄零依賴，**openai SDK ＞ litellm**（20 vs 52 依賴、純 wheel、無自帶 Rust；使用者只打 OpenAI-compatible 端點，litellm 的多-provider 統一層是白付重量）。
- **中文輸出**：預設 cp950；使用者已設 User 級 `PYTHONUTF8=1`（新開終端生效）。Git Bash 管道下 stdout 非主控台會退回 cp950，帶 `PYTHONUTF8=1` 保險。
