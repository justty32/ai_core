## §6 執行保證

### metadata fields

| key | 型別 | 說明 |
|---|---|---|
| `guarantee` | string | 程式對執行結果的承諾（狀態一致性） |
| `dry_run` | bool \| object | 是否支援 dry-run 模式（能力宣告） |

**預設值**：
- `guarantee` 缺席或 `null` → `"none"`（呼叫方自行承擔重試或中斷後果）
- `dry_run` 缺席或 `false` → 不支援 dry-run

---

### `guarantee` 說明

描述程式對其執行副作用的承諾，尤其是失敗或中斷後的狀態一致性。

| 值 | 語意 |
|---|---|
| `"none"` | 無承諾（預設值）；重複執行或中途失敗的後果由呼叫方自行承擔 |
| `"idempotent"` | 重複執行與執行一次等效；中斷後安全重試，不會累積額外副作用 |
| `"transactional"` | 全部成功或完全不發生（ACID 語意）；中途失敗自動回滾，不留部分狀態 |

> **與 §3 的關係**：`guarantee` 對 `state: "stateless"` 的程式通常無意義——stateless 程式本身即等同冪等，亦無外部狀態可回滾。有意義的保證宣告限於 `state: "stateful_external"` 的程式。

---

### `dry_run` 說明

宣告程式是否支援 dry-run 模式。開啟時，程式執行完整邏輯但**不實際修改外部狀態**，用於預覽副作用。

布林形式（支援，flag 名稱由程式自訂）：

```json
"dry_run": true
```

物件形式：

```json
"dry_run": {
  "flag": "--dry-run",
  "state_entry": "stdout",
  "error_entry": "stderr"
}
```

| 子欄位 | 型別 | 說明 |
|---|---|---|
| `flag` | string | 觸發 dry-run 模式的 CLI flag（選填）|
| `state_entry` | string | dry-run 執行時，輸出「若正式執行會發生什麼」的 entry 名稱；引用 `entries` 中的 key（選填） |
| `error_entry` | string | dry-run 執行時，輸出錯誤訊息的 entry 名稱；引用 `entries` 中的 key（選填） |

三個子欄位皆選填；物件形式中至少應填一個有意義的欄位，否則用布林形式即可。

> **注意**：`dry_run` 是能力宣告（程式支援此模式），而非執行時的當前狀態。呼叫方依此決定是否傳入對應 flag。

`dry_run` 與 `guarantee` 正交——兩者可同時存在：
- `guarantee: "transactional"` + `dry_run` → 正式執行具事務保證，且支援預覽
- `guarantee: "none"` + `dry_run` → 正式執行無保證，但可先用 dry-run 確認副作用
- `guarantee: "idempotent"` 無 `dry_run` → 冪等保證，呼叫方可直接重試而無需預覽

---

### 與 §5 可中斷性的關係

§5 描述「程式被中斷時的能力」；§6 描述「程式對執行結果的承諾」。兩軸獨立，但搭配使用時語意更完整：

| §5 `interruptible` | §6 `guarantee` | 中斷後行為 |
|---|---|---|
| `"unsafe"` | `"none"` | 外部狀態可能損毀或未完成；呼叫方需自行決定是否重試 |
| `"unsafe"` | `"idempotent"` | 可安全重試；重跑不會疊加副作用 |
| `"unsafe"` | `"transactional"` | 程式自身錯誤會自動回滾；但外部強制終止（kill）可能繞過回滾機制，外部狀態損毀風險仍存在 |
| `"safe"` | 任意 | 中斷無風險；§6 的承諾在 stateless 程式上意義較小 |
| `"rollback"` | `"transactional"` | 兩軸同向——程式對「中斷能回滾」與「執行具事務性」都做出承諾 |

---

### register() 範例

```python
ai_core.register(
    lifecycle="one_shot",
    state="stateful_external",
    guarantee="transactional",
    dry_run={
        "flag": "--dry-run",
        "state_entry": "stdout",
        "error_entry": "stderr",
    },
)
```

`--metadata` 輸出：

```json
{
  "lifecycle": "one_shot",
  "state": "stateful_external",
  "guarantee": "transactional",
  "dry_run": {
    "flag": "--dry-run",
    "state_entry": "stdout",
    "error_entry": "stderr"
  }
}
```

---

### 完整範例

```python
ai_core.register(
    lifecycle="persistent",
    resources={
        "memory": {"startup": "2gb", "idle": "500mb", "peak": "8gb"},
        "gpu": {"vram": "6gb"},
        "time": {"expected": "30s"},
        "llm_entry": True,
    }
)
```

`--metadata` 輸出：

```json
{
  "lifecycle": "persistent",
  "resources": {
    "memory": {"startup": "2gb", "idle": "500mb", "peak": "8gb"},
    "gpu": {"vram": "6gb"},
    "time": {"expected": "30s"},
    "llm_entry": true
  }
}
```
