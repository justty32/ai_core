# util — handy 的通用共用 lib

← [handy](../)

「很多 `.py` 共用的小工具庫」——純函式、無副作用、不綁特定住戶。住戶靠它們，別各自重造。

**它是 Python 3.3+ 隱式命名空間套件**：不需要 `__init__.py`，往裡丟一個 `.py` 就多一個子模組
（`util/config.py` → `util.config`、日後 `util/http.py` → `util.http`…）。

| 模組 | 用途 |
|---|---|
| [`util.config`](config.py) | 讀 JSON ＋ 解析 `$env`／`$ref`（見下）|
| [`util.env`](env.py) | 環境變數小工具：`first()`／`stem()`（見下）|
| [`util.llm`](llm/) | **LLM 地基**：litellm 包裝 → **[llm/README.md](llm/README.md)** |

> `util/llm/` 是唯一有 `__init__.py` 的成員——它是**真的套件**（多檔、有內部相依），不是單檔模組。

## 住戶怎麼吃

把 `sys.path` 指到 **handy 根**（`util` 的父層）再正常 import：

```python
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)          # 住戶就在 handy 根：HERE 即 handy/
from util import config           # 之後 config.read(...)
```

住戶若在子資料夾（folder-as-callable），把路徑指到 handy 根即可。

> 另有 `importlib.util.spec_from_file_location(...)` 能「按路徑載入單檔、不放 path」——但樣板多、
> 內部互 import 麻煩，**只在載入非固定位置的單檔時才划算**；共用 lib 走上面的命名空間套件路。

---

## util.config — 讀 JSON ＋ 解析 `$env` / `$ref`

```python
from util import config
cfg = config.read("some.json")     # 回「完全解析後」的 dict
```

設定檔裡**任意欄位值**的位置都能放**指令式節點**，`read()` 就地替換成解析結果：

| 節點 | 意思 |
|---|---|
| `{"$env": "VAR"}` | 取環境變數 `VAR` |
| `{"$env": "VAR", "default": …}` | 同上，未設時用 `default` |
| `{"$ref": "a.b.c"}` | 引用本檔其他值（從根算 dotted 路徑）|

**用意**：金鑰用 `$env` 從環境帶、不寫死進版控；重複值（共用端點等）用 `$ref` 收斂成一處。

### 實測行為

| 情況 | 結果 |
|---|---|
| `$env` 有設 | 該值 |
| `$env` **未設**、無 `default` | **`None`** ⚠ 見下 |
| `$env` 未設、有 `default` | `default` |
| `$env` 設成**空字串** | 當作沒設 → 走 `default`／`None` |
| `$ref` 指向巢狀路徑 `a.b.c` | 該值（任意型別，不限字串）|
| `$ref` 指向的節點自己含 `$env` | **遞迴解析**，拿到最終值 |
| `$ref` 形成迴圈 | `ValueError: config $ref 迴圈：…` |
| `$ref` 路徑不存在 | `ValueError: config $ref 找不到路徑：…` |
| 同一個 dict 同時有 `$env` 與 `$ref` | **`$env` 優先**（先檢查）|
| 指令式節點在**陣列裡** | 一樣會解析 |

> ⚠ **`$env` 沒設時，那個鍵仍然存在、值是 `None`**——不是被移除。呼叫端要自己判斷，
> 例如 [llme.py](../llme.py) 用 `if value:` 跳過沒填的欄位。

`read()` 在**檔案不存在或 JSON 壞掉時直接拋**（`OSError`／`json.JSONDecodeError`），不自己吞。
另有 `config.resolve(value, root)` 可單獨解析已在記憶體裡的結構。

---

## util.env — 環境變數小工具

純讀 `os.environ`、無副作用。

```python
from util import env

env.first("LLME_KEY_DEEPSEEK", "DEEPSEEK_API_KEY")   # 第一個「有設且非空」的值
env.stem("deep-seek")                                 # → "DEEP_SEEK"
```

| 函式 | 行為 |
|---|---|
| `first(*keys)` | 依序找，回第一個**有設且非空**的值；**空字串視為沒設**；都沒有回 `None` |
| `stem(name)` | 名字→env 慣例識別子：大寫、非英數一律轉 `_`（`gpt-4o.mini` → `GPT_4O_MINI`）|

> ⚠ **目前沒有任何住戶在用**。它原本是 llme 的 api_key 環境變數自動注入（`LLME_KEY_<EP>` ＞
> `<EP>_API_KEY`）在用，那功能已被砍掉。留著給日後住戶，或哪天確定不需要就刪。
