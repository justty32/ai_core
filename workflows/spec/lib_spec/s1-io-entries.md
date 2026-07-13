## §1 I/O 出入口（entries）

### metadata field

| key | 型別 | 說明 |
|---|---|---|
| `entries` | object | key 為自由命名的語意名稱，value 為該 entry 的屬性物件 |

**預設值**：若 `entries` 缺席，等同宣告單一 stdio entry（batch、text、雙向）。

---

### entry 屬性規格

每個 entry 的 value 為一個物件，包含以下欄位：

#### `able_in` / `able_out`

| key | 型別 | 說明 |
|---|---|---|
| `able_in` | bool | 此 entry 是否接收外部資料（輸入方向）|
| `able_out` | bool | 此 entry 是否向外輸出資料（輸出方向）|

兩者可同時為 `true`（雙向，如互動式 entry）。

#### `mode`

描述資料流動形式。可為字串或物件。

**字串形式：**

| 值 | 語意 |
|---|---|
| `"batch"` | 一次性交換：輸入讀完後才開始處理，輸出一次寫完 |
| `"streaming"` | 持續性流動：資料逐筆產生或消費 |
| `"interactive"` | 執行途中需要對方即時介入（如 stdin prompt） |

**物件形式（用於 streaming 需要描述速率）：**

```json
"mode": {
  "type": "streaming",
  "rate": "20b/s"
}
```

```json
"mode": {
  "type": "streaming",
  "chunk_size": "4kb",
  "interval": "500ms"
}
```

物件形式中 `type` 為必填，其餘速率欄位為選填。

#### `type`

描述資料的內容格式。可為字串或物件。

**字串形式：**

| 值 | 語意 |
|---|---|
| `"text"` | 純文字，編碼未指定（呼叫方自行假設） |
| `"binary"` | 二進位資料 |

**物件形式（需要指定編碼或 MIME）：**

```json
"type": {"base": "text", "encoding": "utf-8"}
"type": {"base": "binary", "mime": "image/png"}
```

#### `format`（選填）

描述資料的**結構化格式**，疊在 `type` 之上：`type` 說內容是 text/binary，`format` 說這段 text
其實是結構化資料。可為字串或物件。

**字串形式：**

| 值 | 語意 |
|---|---|
| `"json"` | 整段內容是一筆 JSON 值 |
| `"ndjson"` | 一行一筆 JSON（streaming / 行協議常用） |

**物件形式（非預定義格式的逃生口）：**

```json
"format": {"type": "csv", "delimiter": ","}
```

物件形式中 `type` 為必填，其餘欄位自由。

> 與 `type` 的分工：非文字內容（圖片等）由 `type: {"base": "binary", "mime": "image/png"}`
> 描述；`format` 只負責「文字內容的結構層」。兩者正交。

#### `schema`（選填）

JSON Schema 物件，描述**單筆資料的結構**（`format: "ndjson"` 時逐行適用）。

```json
"schema": {
  "type": "object",
  "properties": {"answer": {"type": "string"}},
  "required": ["answer"]
}
```

- validation 只確保它是 dict，內容不驗證（低限制：LLM 與確定性程式都讀得懂即可）。
- 慣例上搭配 `format: "json"` / `"ndjson"` 使用，但不強制。
- **這個欄位承重兩件事**：(1) 解「接縫 typing」——組合調用鏈時，下游能靜態知道上游輸出長什麼樣；
  (2) 它是日後 **deopt guard 的原料**——固化物的輸出過不過 schema，就是最便宜的執行期守衛
  （見 `workflows/notes/20260702-2003-回到初心-llm-as-function.md` §13.2）。

#### `channel_constraint`（選填）

宣告此 entry 對可接受 channel 的品質下限。mapping 層依此過濾。

| 值 | 語意 |
|---|---|
| `"stable"` | 只接受穩定通道（file、stdio）；不接受 http / socket 等有延遲或中斷風險的通道 |
| 缺席 | 無限制，任何通道皆可 |

#### `terminal_binding`（選填）

僅在 terminal 環境下有意義。描述此 entry 在 CLI 中如何被接線。

```json
"terminal_binding": {
  "argflag": "--input",
  "default": "stdin"
}
```

| key | 型別 | 說明 |
|---|---|---|
| `argflag` | string | 對應的 argparse flag（如 `"--input"`） |
| `default` | string | 未指定 flag 時的預設接線（如 `"stdin"`、`"./data.txt"`） |

換環境時，`terminal_binding` 整塊忽略即可，entry 語意不受影響。

---

### 完整範例

```python
ai_core.register(
    lifecycle="persistent",
    state="stateful_external",
    entries={
        "entry_states": {
            "able_in": False,
            "able_out": True,
            "mode": {"type": "streaming", "rate": "20b/s"},
            "type": "text",
        },
        "entry_trigger": {
            "able_in": True,
            "able_out": False,
            "mode": "batch",
            "type": "text",
            "channel_constraint": "stable",
        },
        "entry_content": {
            "able_in": True,
            "able_out": False,
            "mode": "batch",
            "type": "binary",
            "terminal_binding": {
                "argflag": "--content",
                "default": "./data.bin",
            },
        },
    },
)
```

`--metadata` 輸出：

```json
{
  "lifecycle": "persistent",
  "state": "stateful_external",
  "entries": {
    "entry_states": {
      "able_in": false,
      "able_out": true,
      "mode": {"type": "streaming", "rate": "20b/s"},
      "type": "text"
    },
    "entry_trigger": {
      "able_in": true,
      "able_out": false,
      "mode": "batch",
      "type": "text",
      "channel_constraint": "stable"
    },
    "entry_content": {
      "able_in": true,
      "able_out": false,
      "mode": "batch",
      "type": "binary",
      "terminal_binding": {"argflag": "--content", "default": "./data.bin"}
    }
  }
}
```

---

### Terminal 環境預定義 entry 名稱

> 此節專屬 terminal（CLI / shell）環境。在其他環境中，這些名稱無特殊意義。

使用以下預定義名稱時，其屬性應符合標準定義：

| 名稱 | `able_in` | `able_out` | `mode` | `type` | 說明 |
|---|---|---|---|---|---|
| `stdin` | `true` | `false` | `"batch"` 或 `"interactive"` | `"text"` | 標準輸入流 |
| `stdout` | `false` | `true` | `"batch"` 或 `"streaming"` | `"text"` | 標準輸出流 |
| `stderr` | `false` | `true` | `"streaming"` | `"text"` | 標準錯誤輸出 |

規則：

- `mode` 的具體值仍需在 entry 中明確宣告
- `stderr` 通常不需要出現在 entries 中（屬程式內部的診斷輸出，呼叫方無需感知）
- 檔案型入出口（如 `--input file.txt`）使用自訂名稱，並搭配 `channel_constraint: "stable"` 及 `terminal_binding`；無預定義名稱
- 其他環境（http、socket 等）的預定義名稱留待各環境規範定義
