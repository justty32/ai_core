# SESSION-LOG — handy 進度日誌

← [README](README.md)｜[AGENTS.md](../../AGENTS.md)

**只放「還沒完成」的活狀態**（in-flight / open）；完成即整條刪除、里程碑史交給 git log。
文末「開發環境」節是例外——本機 durable 注記，長期保留。

## 最新進度（open）

> **現況**（2026-07-21）：pllm 已封存，LLM 地基＝`util/llm/`（litellm 薄包裝），真雲端與串流都實測通過。
> 結構與坑一律看 [README](README.md) → [util/README](util/README.md) → [util/llm/README](util/llm/README.md)，
> **這裡只留還沒完成的**。

- **本輪定位＝試裝試打 litellm，「零相依」暫時擱置**（2026-07-21，使用者裁示）。`util.llm` 依賴 litellm 與 AGENTS.md 鐵律 1 相衝，但現階段**不是拍板的架構決定**：**別為此改頂層 AGENTS.md，也別再拿這條出來當待辦問**。等試出體感再談 litellm 去留。
- **`--schema` 的靜默失效已查明兇手＝後端，不是 litellm**（2026-07-21 真後端翻案，詳見 [util/llm/README](util/llm/README.md) 坑 1）。攔下 litellm 實際送出的 body 確認 `response_format` **完整送出**；是 OpenRouter 那個模型的 `supported_parameters` 沒有它、收下直接無視。`drop_params` 已依使用者裁示留在 `False`（立場問題，對此症狀幾乎 no-op）。**尚未做的**：目前只能靠人工查模型表防身，要不要在 `util.llm` 加一層「宣告能力比對」的護欄未議。
- **C++ cllm 的非 Python 消費者現況待盤**，盤完才能定退休時程（脈絡：CLI 即天然跨語言接口，FFI 在 LLM 場景優勢蒸發）。
- ⚠ **OpenRouter 免費 slug 汰換很快**：`tencent/hy3:free` 已轉付費、`google/gemma-4-31b-it:free` 上游限流。要重測先查 `https://openrouter.ai/api/v1/models` 挑當下 `pricing` 真的為 0 的。`llme.json` 的 `openrouter` 預設現為 **`poolside/laguna-m.1:free`**（2026-07-21 使用者指定；262k context、實測基本呼叫與 `--schema` 都通，但它**沒宣告** `response_format`，schema 是靠模型自願配合、非保證，且是 reasoning 模型＝別設小的 `--max-tokens`，見坑 8）。

## 開發環境（本機 durable 注記 · 兩台輪流）

> ⚠ 本專案定位為 **POSIX**（AGENTS.md 鐵律），以下純屬使用者兩台開發機的實測約束，非專案目標平台。
> 使用者**在 Windows 11 與 Manjaro Linux 兩台之間輪流開發**，所以住戶的啟動方式必須兩邊都成立。

### 住戶怎麼啟動：shebang ＝ `uv run --script` ＋ PEP 723（2026-07-21 定）

`llme.py`／`tick.py` 的檔頭是：

```python
#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["litellm"]
# ///
```

**依賴寫在檔案自己身上**，`./llme.py …` 直接可跑，不必先建 venv——這是為了兩台輪流：新機器只要有 `uv`
就開工。Linux 這台實測：首次 15s（下載 48 個套件），之後 warm 起動 <0.1s。

- **⚠ 別寫成 `#!uv run`**：Linux kernel 的 `#!` 只吃**絕對路徑**的直譯器，`uv` 是相對名 →
  execve 直接 `ENOENT`。zsh 會自己接手解析 shebang 幫你圓場，所以互動下看起來會動，但一旦被
  bash script／Makefile／別的程式 exec 就炸（README 教的 `ln -sf llme.py ~/.local/bin/llme` 正是此路徑）。
  必須 `#!/usr/bin/env -S uv run --script`（`env -S` 才能塞多個字）。
- **⚠ Windows 那台要先設 `UV_PYTHON`**：`uv run --script` 預設會挑 uv 自管的 standalone python
  ——就是下面那份 **ssl 壞掉**的。把 `UV_PYTHON` 設成官方安裝器版路徑
  （`$LOCALAPPDATA/Programs/Python/Python313/python.exe`）再用 shebang 啟動；否則就繼續走下面的 `.venv` 路線。
- `requires-python` 上限 `<3.14` 是刻意的，理由見下面「Python 3.14 太新」。

### Windows 11 這台

**跑「連網 python」的可行配置**（2026-07-21 血淚實測）：

- **應用程式控制策略封鎖非簽名 openssl DLL**：uv 自管的 python-build-standalone 版 `libssl-3-x64.dll` 被擋 → `import ssl` 失敗 → **連不了任何 https**（litellm / openai / pllm 真連 API 全掛；只有 file:// 離線 fixture 不受影響）。
- **官方 python.org 簽名版 ssl 可用**（OpenSSL 3.0.15 通過）→ **解釋器必用官方安裝器版，不能用 uv 自管 standalone**。
- **可行鏈路**：官方簽名 python（提供可用 ssl）＋ uv 當 venv/套件管理器（`--python` 指官方版路徑）＋ SDK。實測 `openai 2.46.0` 純 wheel 秒裝、`httpx` TLS client 通。
- **已備好的 venv：`handy/.venv`**（2026-07-21 建，頂層 `.gitignore` 的 `.venv/` 已忽略）。建法與用法：
  ```sh
  uv venv --python "$LOCALAPPDATA/Programs/Python/Python313/python.exe" .venv   # 官方安裝器版
  uv pip install --python ./.venv/Scripts/python.exe --only-binary :all: litellm
  ./.venv/Scripts/python.exe llme.py local 你好      # 跑住戶就用這支解釋器
  ```
  裝到的是 **litellm 1.91.4**——`--only-binary :all:` 讓 resolver 退到 1.93 之前，正好避開自帶 `litellm-rust`（需 cargo 現場編譯）。升級前先確認這點。
- **⚠ uv 陷阱**：`uv python find` 預設優先挑自管 standalone（ssl 壞的那份）。解法＝卸掉它（`uv python uninstall`）或每次 `--python` 顯式指官方安裝器版的路徑（`$LOCALAPPDATA/Programs/Python/Python313/python.exe`）。
- **Python 3.14 太新**：litellm/openai 的 Rust 依賴（pydantic-core / jiter）無 3.14 wheel → **用 3.13**（wheel 生態齊、handy 的 3.11+ 也吃）。
- **litellm 1.93+ 自帶 `litellm-rust`**：需現場編譯（無 cargo 則失敗）；`uv pip install --only-binary :all:` 可退到有純 wheel 的版本繞過。
- **SDK 選型**：若放棄零依賴，**openai SDK ＞ litellm**（20 vs 52 依賴、純 wheel、無自帶 Rust；使用者只打 OpenAI-compatible 端點，litellm 的多-provider 統一層是白付重量）。
- **中文輸出**：預設 cp950；使用者已設 User 級 `PYTHONUTF8=1`（新開終端生效）。Git Bash 管道下 stdout 非主控台會退回 cp950，帶 `PYTHONUTF8=1` 保險。

### Manjaro Linux 這台

- **沒有 `.venv`、系統 python 也沒裝 litellm**——不需要：走上面的 shebang（`uv run --script`）即可，
  uv 自己備環境。`uv` 在 `/usr/bin/uv`。
- ssl 沒有 Windows 那種簽名封鎖問題，uv 自管 python 可用 → **不必設 `UV_PYTHON`**。
- 實測 uv 解到 **python 3.12 ＋ litellm 1.8x 系列純 wheel**，無 cargo 編譯問題。
- 離線冒煙 `python util/llm/test/smoke.py` 用系統 python 就能跑（不需 litellm）：現況 49/49。
