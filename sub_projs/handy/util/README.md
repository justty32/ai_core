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
| `{"$ref": "#a/b/c"}` | 引用**本檔**片段（`/` 分段）|
| `{"$ref": "other.json"}` | 引用**他檔**整份（路徑相對本檔目錄）|
| `{"$ref": "other.json#a/b"}` | 引用他檔的片段 |
| `{"$ref": ["a.json", "b.json"]}` | **依序解析後合併，後者覆蓋前者** |

**用意**：金鑰用 `$env` 從環境帶、不寫死進版控；重複值（共用端點等）用 `$ref` 收斂成一處；
陣列形式用來做「基底＋覆寫」的分層設定。

> **`$ref` 語法對齊 [guanyu 專案的 `util_jsonschema.go`](file:///C:/code/guanyu/project_template/util/util_jsonschema.go)**，
> 跨專案同一套規則。**沒有 `#` 就整串當檔名**——所以引用本檔片段一定要寫 `#`。

### 陣列形式的合併規則

依序解析每個引用，用後者覆蓋前者：

- **兩邊都是 object → 遞迴深合併**（後者只覆蓋它指定的鍵，其餘保留前者）
- **任一邊不是 object**（陣列、純量）→ **後者整個取代前者**

```jsonc
// a.json  {"host": "h1", "opt": {"x": 1, "y": 1}}
// b.json  {"host": "h2", "opt": {"y": 9, "z": 9}}
{"$ref": ["a.json", "b.json"]}
// → {"host": "h2", "opt": {"x": 1, "y": 9, "z": 9}}
```

### 實測行為

| 情況 | 結果 |
|---|---|
| `$env` 有設 | 該值 |
| `$env` **未設**、無 `default` | **`None`** ⚠ 見下 |
| `$env` 未設、有 `default` | `default` |
| `$env` 設成**空字串** | 當作沒設 → 走 `default`／`None` |
| `$ref` 片段開頭的 `/` | 可有可無（`#a/b` 同 `#/a/b`）|
| `$ref` 片段用數字段 | 可索引陣列（`#list/1`）|
| `$ref` 指向的節點自己含 `$env`／`$ref` | **遞迴解析**，拿到最終值 |
| 同一份檔被引用多次 | **只讀一次**（依絕對路徑 cache）|
| 同一個 dict 同時有 `$ref` 與 `$env` | **`$ref` 優先**（同 Go 版）|
| 指令式節點在**陣列裡** | 一樣會解析 |
| `$ref` 形成迴圈 | `ValueError: config $ref 迴圈：…` |
| `$ref` 片段不存在 | `ValueError: config $ref 找不到路徑：…` |
| `$ref` 陣列為空／元素非字串／`$ref` 非字串非陣列 | 各拋對應的 `ValueError` |

> ⚠ **`$env` 沒設時，那個鍵仍然存在、值是 `None`**——不是被移除。呼叫端要自己判斷，
> 例如 [llme.py](../llme.py) 用 `if value:` 跳過沒填的欄位。
>
> ⚠ 這點與 Go 版不同：**Go 在環境變數未設時直接報錯**，這裡回 `default`／`None`。

`read()` 在**檔案不存在或 JSON 壞掉時直接拋**（`OSError`／`json.JSONDecodeError`），不自己吞。
另有 `config.resolve_in(js, base_dir)` 可解析已在記憶體裡的結構（相當於 Go 的 `ResolveJSRefsIn`）。

**自測**：`python util/test/config_smoke.py`（離線、零相依）；現況 24/24。

---

## util.jsref — `$ref` 語法的純函式

`config` 的下層，不碰檔案、不遞迴：`split_ref()` 拆「檔案#片段」、`lookup()` 沿片段路徑取值
（回 `(值, 有沒有找到)`，好分辨「不存在」與「值是 null」）、`merge()` 深合併兩個值。
單獨拿來合併兩個 dict 也行。

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
