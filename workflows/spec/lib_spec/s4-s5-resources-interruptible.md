## §5 可中斷性

### metadata field

| key | 型別 | 允許值 |
|---|---|---|
| `interruptible` | string \| object | 見下表 |

**預設值**：若此 key 缺席、為 `null` 或 `false`，等同 `"unsafe"`（最保守）。

---

### 說明

描述程式對中斷（強制退出、外部 kill、環境切換）的承受能力，以及中斷後對外部狀態的影響。此軸為跨環境的抽象描述，不依賴具體的 OS 信號或平台機制。

> **設計備註**：「損毀外部狀態」的定義是廣義的——不只是寫到一半造成資料損毀，也包括「程式被中斷、任務未正常完成，因此外部狀態未達到預期結果」。後者屬於任務層面的不完整，同樣是呼叫方需要知道的風險。相關定義之後可能需要精心深挖與設計。

**字串形式（標準值）：**

| 值 | 語意 |
|---|---|
| `"safe"` | 可隨時中斷；無副作用，中斷後外部狀態不受影響 |
| `"unsafe"` | 中斷可能損毀外部狀態，或任務未完成導致狀態未達預期（預設值） |
| `"resettable"` | 中斷損毀狀態，但程式提供重置機制可恢復至某個安全狀態（不保證回到原始狀態） |
| `"rollback"` | 中斷損毀狀態，但程式提供回滾機制可撤銷所有未完成的修改，完整還原至呼叫前的原始狀態 |
| `"resumable"` | 可中斷，且程式支援從斷點繼續執行；中斷不損毀狀態，下次呼叫從中斷處接續，而非從頭開始 |
| `"graceful"` | 可中斷，但不能直接 kill；需給程式時間完成 cleanup 後正常退出 |

**物件形式（需補充細節，或非標準情形）：**

```json
{"type": "resettable", "reset_hint": "--reset"}
```

```json
{"type": "graceful", "timeout": "5s"}
```

```json
{"type": "conditional", "condition": "llm_entry 正常運作時可隨時中斷"}
```

物件形式中 `type` 為必填，其餘選填。`"conditional"` 及其他非標準型別屬自定義，value 格式不受限制。

---

### 與 §3 / §6 的關係

| 軸 | 職責 |
|---|---|
| §3 跨呼叫狀態 | 描述程式有無跨執行的外部狀態 |
| §5 可中斷性 | 描述中斷對當次執行與外部狀態的影響 |
| §6 執行保證 | 描述中斷後如何恢復（idempotent / transactional） |

`"safe"` 通常與 `state: "stateless"` 搭配；但兩者獨立——stateless 程式執行途中仍可能有未提交的外部寫入，此時 `interruptible` 應為 `"unsafe"` 或 `"graceful"`。

---

### register() 範例

```python
ai_core.register(
    lifecycle="persistent",
    state="stateful_external",
    interruptible={"type": "resettable", "reset_hint": "--reset"},
)
```

`--metadata` 輸出：

```json
{
  "lifecycle": "persistent",
  "state": "stateful_external",
  "interruptible": {"type": "resettable", "reset_hint": "--reset"}
}
```

---

## §4 資源特性

> **設計備註**：`resources` 採自由 key-value 結構。標準規範精確定義最常見的具體資源 key（memory / gpu / time / disk / network）；其定義固定。自定義 key 的 value 格式不受限制。

### metadata field

| key | 型別 | 說明 |
|---|---|---|
| `resources` | object | key 為資源名稱，value 為該資源的需求描述 |

**預設值**：若 `resources` 缺席，等同宣告無任何資源需求聲明。

---

### 設計原則

`resources` 為自由 key-value 結構，與 `entries` 同模式：

- key 完全自由命名——可以是顯性的系統資源（`memory`、`gpu`），也可以是抽象的隱性依賴（`llm_entry`、`db`）
- **預定義 key**（見下節）有規範的 value 格式，hub 可直接解析
- **自定義 key** 的 value 格式由撰寫者自訂；hub 讀得到但不一定能理解，至少讓呼叫方知道此依賴存在

---

### 預定義 key

#### `memory`

描述記憶體用量。

字串形式（宣告峰值）：

```json
"memory": "4gb"
```

物件形式（需要區分各階段）：

```json
"memory": {"startup": "2gb", "peak": "8gb", "idle": "500mb"}
```

| 子欄位 | 說明 |
|---|---|
| `startup` | 啟動時立即佔用（e.g., 模型載入後常駐） |
| `peak` | 執行途中的記憶體峰值 |
| `idle` | Persistent 程式閒置時的底線佔用 |

三個子欄位皆選填；`idle` 僅對 `lifecycle: "persistent"` 有意義。

#### `gpu`

布林形式（宣告是否需要 GPU）：

```json
"gpu": true
```

物件形式（需要宣告 VRAM 用量）：

```json
"gpu": {"vram": "6gb"}
```

#### `time`

描述執行時間預估。

字串形式（預期執行時間）：

```json
"time": "30s"
```

物件形式（需要宣告上限）：

```json
"time": {"expected": "30s", "max": "5m"}
```

#### `disk`

執行期間的暫用磁碟空間（結束後清除）。持久性資料（§3 `.data` 目錄）不在此描述。

```json
"disk": "500mb"
```

#### `network`

宣告執行期間是否需要網路存取。

```json
"network": true
```

---

### 單位格式

| 種類 | 單位 | 範例 |
|---|---|---|
| 容量 | `b` / `kb` / `mb` / `gb` | `"512mb"`、`"4gb"` |
| 時間 | `ms` / `s` / `m` / `h` | `"500ms"`、`"30s"`、`"5m"` |

---

### 自定義 key 範例

```json
"resources": {
  "llm_entry": true,
  "db": {"type": "postgres"},
  "render_server": {"port": 7860}
}
```

value 格式無限制，由撰寫者自行定義語意。
