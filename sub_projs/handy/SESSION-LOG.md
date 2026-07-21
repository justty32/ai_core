# SESSION-LOG — handy 進度日誌

← [README](README.md)｜[AGENTS.md](../../AGENTS.md)

**只放「還沒完成」的活狀態**（in-flight / open）；完成即整條刪除、里程碑史交給 git log。
文末「開發環境」節是例外——本機 durable 注記，長期保留。

## 最新進度（open）

- **litellm 換腦已落地，但「真的打得通」未驗**（2026-07-21）。使用者拍板：**pllm 封存、改用 litellm**。已完成：
  - `pllm/` → `archive/pllm/`（凍結，可還原）；新地基＝**`util/llm/`**（litellm 薄包裝）。
  - **對外形狀＝舊 pllm**：`ask()` 簽章與整套 CLI 旗標逐項照舊（完整鏡像，非子集），`llme.py` 只改一行 import。
  - 分歧三處（已寫進 README／`call.py` docstring）：endpoint 仍收完整 URL、內部剝成 `api_base`；model 自動加 `openai/` 前綴（`LLM_RAW_MODEL=1` 可關）；`drop_params` 開著。`file://` 保留為**非串流** fixture 逃生口。
  - **踩到並修掉一個真 bug**：litellm 的 openai provider **一律要求 api_key**，不給就丟 `InternalServerError`——連本機免認證端點都打不出去（`llme local`／`qwen` 首當其衝，舊 pllm 無 key 就不送 header 故無此限）。修法＝沒給 key 時補佔位字串 `handy-no-auth`，已加迴歸測試守門。
  - 驗證：`python util/llm/test/smoke.py` 離線 30/30 過（裝與沒裝 litellm 兩種解釋器都過）；另起 stdlib 假後端做端到端實跑，**非串流／串流／system＋schema＋取樣參數全通**，比對後端實收 body 正確。
  - **open 尾巴**：①**真雲端 API 仍未打過**（待使用者 DeepSeek key）——目前所有實跑都對本機假後端；②本機 LM Studio 未實測（多離線）；③tool_calls 只驗到 fixture 解析，未經真後端往返；④C++ cllm 非 Python 消費者現況待盤（定退休時程）。
- **零相依鐵律出現第一個例外，待使用者定調**（2026-07-21）。AGENTS.md 鐵律 1「零外部相依」現與 `util.llm` 依賴 litellm 衝突。目前只在 handy/README.md 就地註記為「刻意例外」，**未動頂層 AGENTS.md**。要不要把鐵律改成「地基層可有相依、其餘零相依」之類的措辭，請使用者決定。

## 開發環境（本機 durable 注記 · Windows）

> ⚠ 本專案定位為 **POSIX**（AGENTS.md 鐵律），以下純屬使用者當前 **Windows 11 開發機**的實測約束，非專案目標平台。

**這台 Windows 跑「連網 python」的可行配置**（2026-07-21 血淚實測）：

- **應用程式控制策略封鎖非簽名 openssl DLL**：uv 自管的 python-build-standalone 版 `libssl-3-x64.dll` 被擋 → `import ssl` 失敗 → **連不了任何 https**（litellm / openai / pllm 真連 API 全掛；只有 file:// 離線 fixture 不受影響）。
- **官方 python.org 簽名版 ssl 可用**（OpenSSL 3.0.15 通過）→ **解釋器必用官方安裝器版，不能用 uv 自管 standalone**。
- **可行鏈路**：官方簽名 python（提供可用 ssl）＋ uv 當 venv/套件管理器（`--python` 指官方版路徑）＋ SDK。實測 `openai 2.46.0` 純 wheel 秒裝、`httpx` TLS client 通。
- **已備好的 venv：`handy/.venv`**（2026-07-21 建，頂層 `.gitignore` 的 `.venv/` 已忽略）。建法與用法：
  ```sh
  uv venv --python "C:/Users/WG-Guanyu/AppData/Local/Programs/Python/Python313/python.exe" .venv
  uv pip install --python ./.venv/Scripts/python.exe --only-binary :all: litellm
  ./.venv/Scripts/python.exe llme.py local 你好      # 跑住戶就用這支解釋器
  ```
  裝到的是 **litellm 1.91.4**——`--only-binary :all:` 讓 resolver 退到 1.93 之前，正好避開自帶 `litellm-rust`（需 cargo 現場編譯）。升級前先確認這點。
- **⚠ uv 陷阱**：`uv python find` 預設優先挑自管 standalone（ssl 壞的那份）。解法＝卸掉它（`uv python uninstall`）或每次 `--python` 顯式指官方路徑（`C:/Users/WG-Guanyu/AppData/Local/Programs/Python/Python313/python.exe`）。
- **Python 3.14 太新**：litellm/openai 的 Rust 依賴（pydantic-core / jiter）無 3.14 wheel → **用 3.13**（wheel 生態齊、handy 的 3.11+ 也吃）。
- **litellm 1.93+ 自帶 `litellm-rust`**：需現場編譯（無 cargo 則失敗）；`uv pip install --only-binary :all:` 可退到有純 wheel 的版本繞過。
- **SDK 選型**：若放棄零依賴，**openai SDK ＞ litellm**（20 vs 52 依賴、純 wheel、無自帶 Rust；使用者只打 OpenAI-compatible 端點，litellm 的多-provider 統一層是白付重量）。
- **中文輸出**：預設 cp950；使用者已設 User 級 `PYTHONUTF8=1`（新開終端生效）。Git Bash 管道下 stdout 非主控台會退回 cp950，帶 `PYTHONUTF8=1` 保險。
