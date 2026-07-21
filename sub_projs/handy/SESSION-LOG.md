# SESSION-LOG — handy 進度日誌

← [README](README.md)｜[AGENTS.md](../../AGENTS.md)

**只放「還沒完成」的活狀態**（in-flight / open）；完成即整條刪除、里程碑史交給 git log。
文末「開發環境」節是例外——本機 durable 注記，長期保留。

## 最新進度（open）

- **litellm 換腦已落地，但「真的打得通」未驗**（2026-07-21）。使用者拍板：**pllm 封存、改用 litellm**。已完成：
  - `pllm/` → `archive/pllm/`（凍結，可還原）；新地基＝**`util/llm/`**（litellm 薄包裝）。
  - **對外形狀＝舊 pllm**：`ask()` 簽章與整套 CLI 旗標逐項照舊（完整鏡像，非子集），`llme.py` 只改一行 import。
  - 分歧三處（已寫進 README／`call.py` docstring）：endpoint 仍收完整 URL、內部剝成 `api_base`；model 自動加 `openai/` 前綴（`LLM_RAW_MODEL=1` 可關）；`drop_params` 開著。`file://` 保留為**非串流** fixture 逃生口。
  - 驗證：`python util/llm/test/smoke.py` 離線 29/29 過（CLI 分流＋參數翻譯）。
  - **open 尾巴**：①**litellm 根本還沒裝**（此機無），故「真的送得出請求」零實測——只驗到未裝時報錯；②真連 API 待使用者 DeepSeek key；③**串流路徑完全未測**（fixture 只解非串流）；④C++ cllm 非 Python 消費者現況待盤（定退休時程）。
- **零相依鐵律出現第一個例外，待使用者定調**（2026-07-21）。AGENTS.md 鐵律 1「零外部相依」現與 `util.llm` 依賴 litellm 衝突。目前只在 handy/README.md 就地註記為「刻意例外」，**未動頂層 AGENTS.md**。要不要把鐵律改成「地基層可有相依、其餘零相依」之類的措辭，請使用者決定。

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
