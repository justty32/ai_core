# code_01 — 正式核心 Library (`ai_core`) 說明書

> **一句話講完**：這是整個專案唯一的「正式版」Code，負責把每個 Function 的規格（九軸 metadata）記下來，並在有人下 `--metadata` 指令時把它噴成 JSON 印出來。

---

## 1. 它的地位：唯一真理

這個 Repo 裡 `src/ai_core/_core.py` 是唯一正式定案的東西。其他 `try_implement/` 底下的原型雖然能跑，但都還沒「轉正」。

這 Library 只有一個目的：**強制執行契約**。
不管是 Hub、Indexer 還是 SFC，要能互相溝通，全靠每個 Function 都能乖乖用 `--metadata` 自我介紹。在 `ai_core` 裡，寫 Metadata 是義務，不是福利。

### 設計重點：沒叫你動，你就別動 (No Side Effects)
這是最重要的決定：
- `register*` 系列函式**只負責記帳**。你 Import 它、執行它，它都不會去偷看 `sys.argv`，也不會突然關掉你的程式。
- 只有當你顯式呼叫 `intercept()` 時，它才會去檢查有沒有人下 `--metadata`。有，它就印 JSON 並 `exit`；沒有，它就乖乖把控制權還給你。

---

## 2. 你會用到的 4 個 API

```python
from ai_core import register, register_subcommand, register_subcommand_resolver, intercept
```

### 1. `register(**kwargs)`
告訴系統這個程式的基本資料。
- 沒副作用，可以重複呼叫（最後一次說的才算）。
- 沒給參數也行，那就是空的 Metadata。

### 2. `register_subcommand(name, **kwargs)`
如果你的程式像 `git` 那樣有一堆子指令（例如 `git commit`, `git push`），就用這個分別幫子指令寫規格。

### 3. `register_subcommand_resolver(fn)`
**進階用法**。如果子指令是動態生成的（例如從資料庫讀出來的），你可以註冊一個 Resolver 函式去幫你找。

### 4. `intercept(argv=None)`
**這是守門員**。你通常把它放在 `main()` 的最前面。
- 它會檢查命令列有沒有 `--metadata`。
- 如果有人問 `--metadata`，它印完就直接 `sys.exit(0)`，你的程式後面就不會執行了。
- 如果沒人問，它就當沒事發生。

---

## 3. 「九軸」Metadata 規格

系統會嚴格檢查你填的東西。如果你填錯了（例如打錯字、型別不對），它會直接噴 `ValueError` 或 `TypeError` 給你。

| 軸 (欄位) | 填什麼 | 規則 |
|---|---|---|
| **lifecycle** | `one_shot` / `persistent` | 執行完就死，還是會一直開著？ |
| **state** | `stateless` / `stateful_external` | 是否有狀態？ |
| **entries** | 一個 Dict | 定義輸入輸出（`able_in`, `able_out` 是必填的 bool）。 |
| **state_dirs** | `config`, `cache`, `state`, `data` | 如果有存檔，存在哪？ |
| **resources** | 一個 Dict | 用了多少資源（記憶體、GPU 之類的）。 |
| **interruptible** | `safe`, `unsafe`, `resumable` 等 | 能不能中途被打斷？ |
| **guarantee** | `none`, `idempotent`, `transactional` | 有沒有冪等性（Idempotent）保證？ |
| **dry_run** | `bool` 或 Dict | 支不支持「演習」模式？ |
| **nondeterministic** | `bool` 或 Dict | **這是第 9 軸**。專門處理 LLM 的隨機性。填 True 表示這傢伙會亂跑，填 Dict 可以附上「穩定度證明書」。 |

---

## 4. 硬體約束：零依賴 (Zero Dependency)

核心代碼（`_core.py`）有個死命令：**絕對不能依賴任何第三方套件**。
- 只准用 Python 3.11+ 內建的東西。
- 沒 `pip install` 也能跑。
- 就連打包設定 `pyproject.toml` 也是乾乾淨淨的。

---

## 5. 測試：65 個測試全綠

在 `tests/test_core.py` 裡有 65 個測試，涵蓋了各種填錯規格、正確攔截、子指令優先順序等情況。
如果你改了核心代碼，記得跑一下：
```bash
python -m pytest tests/test_core.py
```
要把東西弄壞是很難的，因為這些測試盯得很緊。
