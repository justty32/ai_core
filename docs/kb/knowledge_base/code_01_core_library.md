# code_01 — 正式核心 library (`ai_core`)

> **來源**：`src/ai_core/_core.py`（255 行）、`src/ai_core/__init__.py`、`tests/test_core.py`（460 行，65 個測試）、`pyproject.toml`
> **狀態**：定案 / 正式核心（**唯一**的正式程式碼）
> **一行摘要**：純宣告的 `register*()` 把九軸 metadata 寫入全域並做型別/值域驗證，顯式 `intercept()` 命中 `--metadata` 查詢時輸出 JSON 並 exit；其餘交還控制權。

相關文件：契約規範見 [doc_06_lib_contract.md](doc_06_lib_contract.md)；九軸完整定義見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)。

---

## 1. 定位：唯一的正式程式碼

整個 repo 的「正式核心程式碼」**只有一處**：`src/ai_core/_core.py`（＋ 其測試 `tests/test_core.py`）。`try_implement/` 下的原型（[code_02](code_02_prototype_tools.md)、[code_03](code_03_prototype_lib.md)、[code_04](code_04_prototype_demos_tests.md)）即使能跑，也**尚未**等於規範定案。

本 library 是**跨元件的硬契約**的落地處：Function Hub、Indexer、SFC 的可行性全建立在「每個函式都能用 `--metadata` 輸出 JSON 自述」之上。實作任何新函式時，metadata 不是可選項。

### 設計核心：純宣告 / 顯式攔截拆分模型（拒絕 import-time 副作用）

這是本 library 最重要的設計決策（DECISIONS A1/A2/A3 已收斂）：

- `register*` 系列**只「宣告」**——純粹寫入三個模組層級全域，**不讀 `sys.argv`、不攔截 `--metadata`、不 `sys.exit`**。因此工具可被當 library `import` 而無副作用。
- `--metadata` 的攔截由 **`intercept()` 顯式負責**，需在 `main()` / `__main__` 區塊明確呼叫。
- 慣例：`register*` 因無副作用、import 即安全，但仍應在 `__main__` / `main()` 呼叫；`--metadata` 生效靠顯式 `intercept()`，不靠 import 副作用。

模組層級的三個全域登記表：

```python
_top_metadata: dict[str, Any] = {}                  # register() 寫入
_subcommands: dict[str, dict[str, Any]] = {}        # register_subcommand() 寫入
_resolver: Callable[[str, str | None], dict | None] | None = None  # register_subcommand_resolver() 寫入
```

---

## 2. Public API（逐一）

`__init__.py` 從 `_core` 匯出且 `__all__` 列出的四個名稱（**僅此四個為 public**；其餘 `_validate*`、`_emit`、全域變數均為私有）：

```python
__all__ = ["register", "register_subcommand", "register_subcommand_resolver", "intercept"]
```

### `register(**kwargs) -> None`

宣告程式頂層 metadata（dispatcher 的預設行為）。

- **行為**：純宣告、零副作用。把 `kwargs` 交給 `_validate()` 驗證後存入 `_top_metadata`（`_top_metadata = _validate(kwargs)`，整體覆寫）。
- **回傳**：`None`（測試 `test_register_returns_none` 明確驗證）。
- **可重複呼叫**：採 **last-write-wins**（`test_register_twice_last_write_wins`）。注意舊契約曾是「二次呼叫 raise」，拆分模型後改為純宣告覆寫。慣例上每個程式只在 `__main__` 呼叫一次。
- **無參數合法**：`register()` → `_top_metadata == {}`。
- **例外**：未知欄位 → `ValueError("unknown metadata fields: ...")`；各軸值非法依下節規則 raise `ValueError`/`TypeError`。

### `register_subcommand(name: str, **kwargs) -> None`

宣告某個**靜態子命令**的 scoped metadata（解「單一執行檔含多種 lifecycle 子命令」——頂層描述 dispatcher 預設，各子命令各自覆寫）。

- **行為**：`_subcommands[name] = _validate(kwargs)`。同樣經完整九軸驗證。
- `intercept()` 會處理 `prog <name> --metadata`。
- **回傳**：`None`。

### `register_subcommand_resolver(fn) -> None`

註冊**動態子命令解析器**，簽名 `fn(name: str, store_override: str | None) -> dict | None`。

- **用途**：子命令名稱來自外部資料（如 SFC 的 tiny function 來自 store）時，靜態登記查不到才交給 resolver；回傳 `None` 表示查無此子命令。
- **行為**：存入全域 `_resolver`（`global _resolver`）。
- **回傳**：`None`。

### `intercept(argv: list[str] | None = None) -> None`

放寬版 `--metadata` 攔截。**命中** metadata 查詢則輸出 JSON 並 `sys.exit`；**否則 `return None`** 交還控制權。

- `argv` 預設 `None` → 取 `sys.argv[1:]`（`test_intercept_defaults_to_sys_argv`）。
- **前置處理**：先吃掉可選的前導 `--store DIR`（`work[0] == "--store"` 時取 `work[1]` 為 `store_override`、`work = work[2:]`），使 `prog --store DIR <sub> --metadata` 也成立。
- 攔截三規則：

  1. **`work == ["--metadata"]`** → `_emit(_top_metadata)`：印頂層 metadata JSON，`exit 0`。
  2. **`len(work) == 2 and work[1] == "--metadata"`**（即 `<name> --metadata`）→ 依序查：
     - 靜態 `_subcommands[name]` 命中 → `_emit`，`exit 0`；
     - 否則若有 `_resolver`，呼叫 `_resolver(name, store_override)`，非 `None` 命中 → `_emit`，`exit 0`；
     - 都查無 → stderr 印 `--metadata: unknown subcommand/function 'name'`，`sys.exit(1)`。
     - **靜態優先於 resolver**（`test_intercept_static_takes_precedence_over_resolver`）。
  3. **其餘** → `return`（一般 dispatch，無輸出、不 exit，控制權交回 caller）。

- **輸出機制**：私有 `_emit(md)` = `print(json.dumps(md, ensure_ascii=False))` 後 `sys.exit(0)`（`ensure_ascii=False` 確保中文不被轉義）。
- **例外/退出碼**：命中查詢 → `SystemExit(0)`；`<name> --metadata` 查無 → `SystemExit(1)`；非 metadata 查詢 → 正常 `return None`。

---

## 3. 九軸 metadata 與驗證規則

`_KNOWN_FIELDS`（frozenset，共 9 個欄位 = 九軸）：

```
entries, lifecycle, state, state_dirs, resources, interruptible, guarantee, dry_run, nondeterministic
```

`_validate(kwargs)` 流程：先檢查未知欄位（`set(kwargs) - _KNOWN_FIELDS` 非空 → `ValueError`），再逐軸驗證並重建一個只含已知欄位的 result dict。各軸規則（完整語意見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)）：

| 軸 | 合法值 / 型別 | 驗證細節 | 違規例外 |
|---|---|---|---|
| **lifecycle** | `one_shot` / `persistent`（`_LIFECYCLE_VALUES`） | 列舉成員檢查 | `ValueError` "lifecycle must be one of ..." |
| **state** | `stateless` / `stateful_external`（`_STATE_VALUES`） | 列舉成員檢查 | `ValueError` "state must be one of ..." |
| **state_dirs** | `list`，元素 ∈ `{config, cache, state, data}`（`_STATE_DIR_VALUES`） | 須是 list；`set(v) - 合法值` 非空則報無效值；空 list 合法 | `TypeError`（非 list）/ `ValueError`（含無效值） |
| **entries** | `dict`（見下） | 須是 dict；每個 entry 走 `_validate_entries` | `TypeError`（非 dict） |
| **resources** | `dict`（自由 key-value，如 `memory`/`gpu`/自訂 key/巢狀 dict） | 僅檢查是否為 dict，內容自由 | `TypeError`（非 dict） |
| **interruptible** | `str` ∈ `{safe, unsafe, resettable, rollback, resumable, graceful}` 或 `dict`（須含 `type`） | `_validate_interruptible` | `ValueError`（無效 str / dict 缺 type）/ `TypeError`（非 str/dict，如 int） |
| **guarantee** | `none` / `idempotent` / `transactional`（`_GUARANTEE_VALUES`，偏序 none<idempotent<transactional） | 列舉成員檢查 | `ValueError` "guarantee must be one of ..." |
| **dry_run** | `bool` 或 `dict`（自由內容） | `_validate_dry_run`，型別放行 | `TypeError`（非 bool/dict，如 str） |
| **nondeterministic** | `bool`（未認證留白）或 `dict`（證書：建議 key `model`/`test_set`/`stability`，value 不強制） | `_validate_nondeterministic`，僅檢型別 | `TypeError`（非 bool/dict） |

### entries 子驗證（`_validate_entries` 及輔助）

每個 entry 須是 dict，且：

- **必填**：`able_in`、`able_out`，兩者皆須為 `bool`（缺 → `ValueError`；型別錯 → `TypeError`）。
- **選填 `mode`**（`_validate_entry_mode`）：str ∈ `{batch, streaming, interactive}`，或 dict（須含 `type` key）。其他 → `TypeError`。
- **選填 `type`**（`_validate_entry_type`）：str ∈ `{text, binary}`，或 dict（須含 `base` key）。其他 → `TypeError`。
- **選填 `channel_constraint`**：目前**只定義 `"stable"`**，其他值 → `ValueError`。
- **選填 `terminal_binding`**：須是 dict，否則 `TypeError`。

### 三個型別放行軸的設計意圖

`resources` / `dry_run` / `nondeterministic` 刻意只做型別檢查、不限制內容（從粗糙到嚴整、可自由擴充）。`nondeterministic` 是**第九軸**，是 LLM 馴化框架 / 憑證准入的入口：`True` = 未認證留白（開機期僅標記隨機），`dict` = 證書（用哪個模型、哪個測試組、認證穩定度 %）。

---

## 4. 打包設定（`pyproject.toml`）

- **build backend**：`hatchling`（`requires = ["hatchling"]`）。
- **套件名**：`ai-core`，version `0.1.0`。
- **`requires-python = ">=3.11"`**。
- **`dependencies = []`** — 刻意維持零 runtime 相依（KISS / Least dependency / 只用標準庫的硬約束；`_core.py` 只 import `json`、`sys`、`typing`）。
- **`[dev]` extra**：`pytest>=8.0.0`（唯一開發相依）。
- **wheel target**：`packages = ["src/ai_core"]`（src layout）。
- **無 entry points / console scripts**：本 library 不註冊 CLI 指令；它是被各函式 import 使用的契約 library。

---

## 5. 測試（`tests/test_core.py`，實際 65 個）

> **與 CLAUDE.md 的數字差異（待澄清）**：本任務簡報與 CLAUDE.md 某處稱 65，CLAUDE.md「建置/測試」段又稱 `pytest -q` **目前 82 passed**。實測 `tests/test_core.py` 為 **65 passed**。差距 17 應來自 `tests/` 下其他測試檔或 pytest 收集到的其他項目；本檔負責的 `tests/test_core.py` **確認為 65**。原型 smoke test（`smoke_test.py` 72 項、`lib_smoke_test.py` 68 項，見 [code_04](code_04_prototype_demos_tests.md)）是純標準庫斷言、**不經 pytest**，與這 65/82 無關。

`autouse` fixture `reset` 在每個測試前後把三個全域清空，確保互不污染。

測試覆蓋面（依分類）：

- **register 純宣告行為（6）**：存 metadata、回傳 None、即使 argv 帶 `--metadata` 也不讀 argv/不 exit（拆分模型核心保證）、二次呼叫 last-write-wins、無參數合法、未知欄位 raise。
- **intercept 各分支（4）**：頂層 metadata exit 0、預設取 `sys.argv`、非 metadata 查詢 return None 且無輸出、空 metadata exit 0。
- **subcommand-scoped / resolver（7）**：靜態子命令 metadata（A2）、未知子命令 exit 1、動態 resolver 命中（A1）、resolver miss exit 1、靜態優先於 resolver、`--store` 前綴被吃掉仍命中、resolver 收到 `store_override`。
- **lifecycle（3）**：one_shot / persistent / 非法值。
- **state + state_dirs（8）**：兩個合法值 + 非法值；state_dirs 合法、全四值、空 list、非法值、非 list。
- **entries（17）**：最小合法、完整欄位、streaming dict mode、binary type、type dict 帶 mime；缺 able_in/able_out、able_in 非 bool、mode 非法 str、mode dict 缺 type、type 非法 str、type dict 缺 base、channel_constraint 非法、entries 非 dict。
- **resources（4）**：簡單值、memory 物件、自訂 key/巢狀、非 dict。
- **interruptible（7）**：safe、全六個 str 值、非法 str、dict、dict 缺 type、dict 自訂 conditional type、非 str/dict（int）。
- **guarantee + dry_run（8）**：三個 guarantee 值 + 非法；dry_run True/False/dict/非法型別。
- **nondeterministic（3）**：bool 未認證、證書 dict、非法型別。
- **複合範例（1）**：對應 lib_spec 完整範例，多軸同時宣告後 `intercept(["--metadata"])` 驗證輸出 JSON。

---

## 6. 如何跑測試

```bash
# 正式核心測試（src/ai_core + tests/）
.venv/bin/python -m pytest -q          # CLAUDE.md 稱 82 passed（含 tests/ 全部）
.venv/bin/python -m pytest tests/test_core.py -q   # 本檔範圍：65 passed（實測）
```

repo 內已有 `.venv`；安裝開發相依用 `.venv/bin/pip install -e ".[dev]"`。原型 smoke test 不需 pytest（純標準庫），詳見 [code_04_prototype_demos_tests.md](code_04_prototype_demos_tests.md)。
