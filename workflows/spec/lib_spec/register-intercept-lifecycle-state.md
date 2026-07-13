## `register()` 與 `intercept()`（宣告／攔截拆分）

> **設計演進**：早期版本的 `register()` 在呼叫時就**同時**做「宣告 metadata」與「攔截
> `--metadata`」兩件事，且攔截要求 `--metadata` 必須是唯一引數。這在三種情境下出問題：
> (A) git-style dispatcher 的 `prog <sub> --metadata` 不相容；(B) 單一執行檔含多種 lifecycle
> 的子命令無法各自宣告；(F) 在 module 頂層 `register()` 會在 import 時就讀 `sys.argv` / 攔截 /
> `sys.exit`，使工具無法被當 library 重用。現行規範把**宣告**與**攔截**拆成兩個動作。

### 介面

| 函式 | 職責 | 副作用 |
|---|---|---|
| `register(**kwargs)` | 宣告程式**頂層** metadata（dispatcher 的預設行為） | 無——只寫入內部登記，不讀 argv、不攔截、不 exit |
| `register_subcommand(name, **kwargs)` | 宣告某個**靜態子命令**的 scoped metadata（可與頂層不同 lifecycle） | 無 |
| `register_subcommand_resolver(fn)` | 註冊**動態子命令**解析器 `fn(name, store_override) -> dict \| None`（子命令名稱來自外部資料，如 store） | 無 |
| `intercept(argv=None)` | 顯式攔截 `--metadata`；命中則輸出 JSON 並 `sys.exit`，否則 **return 交還控制權** | 命中 metadata 查詢時 `sys.exit` |

### 規則

1. **`register*` 系列純宣告、無副作用**：可安全地在 import 時或 `main()` 內呼叫，import 不再有
   讀 argv / 攔截 / exit 的副作用（解 F）。
2. **`register()` 可重複呼叫**（last-write-wins，覆寫頂層 metadata）；不再因二次呼叫而 crash。
   慣例上每個程式只在自己以 `__main__` 身分執行時宣告一次。
3. **攔截由 `intercept()` 顯式負責**，須在 `argparse.parse_args()` 之前呼叫。葉子工具的典型形態
   是 `register(...)` 後緊接 `intercept()`。
4. **register 應在 `__main__` / `main()` 內呼叫**（見下方「import-time 副作用」節）。

### `intercept()` 攔截規則

先吃掉可選的前導 `--store DIR`（使 `prog --store DIR <sub> --metadata` 也成立），再依序判斷：

| argv（去掉 `--store DIR` 後） | 行為 |
|---|---|
| `["--metadata"]` | 印**頂層** metadata，`sys.exit(0)` |
| `[<name>, "--metadata"]` | 依序查「靜態子命令登記」→「動態 resolver」；命中印該 **scoped** metadata `exit(0)`；皆查無 → stderr 報錯 `exit(1)` |
| 其餘 | **return**（非 metadata 查詢，交回 caller 走一般 dispatch） |

> 與舊規範的差異：`prog <sub> --metadata` 由「報錯」變成「**合法的 scoped metadata 查詢**」
> （解 A）。`--metadata` 夾在一般引數中間（如 `prog --foo --metadata`）不再特別報錯——除非它
> 落入上表前兩列的形狀，否則一律視為一般引數交還 caller。

### 輸出格式

純 JSON 到 stdout，無 header / wrapper（`ensure_ascii=False`，metadata 可含非 ASCII）：

```json
{"lifecycle": "one_shot"}
```

### 範例

**葉子工具**（單一行為）：

```python
import ai_core

def main():
    ai_core.register(lifecycle="one_shot")
    ai_core.intercept()          # 命中 --metadata 則輸出並 exit；否則往下走

    parser = argparse.ArgumentParser()
    # ...

if __name__ == "__main__":
    sys.exit(main())
```

**dispatcher（含多種 lifecycle 子命令，解 A/B）**：

```python
def main():
    ai_core.register(lifecycle="one_shot", state="stateless")          # 頂層＝dispatcher 預設
    ai_core.register_subcommand("forge", lifecycle="persistent")       # forge 子命令是 server
    ai_core.register_subcommand_resolver(resolve_from_store)           # 動態子命令查 store
    ai_core.intercept()          # 處理 prog --metadata / prog <sub> --metadata 各變體
    # ... 一般 dispatch
```

`prog --metadata` 回頂層 `one_shot`、`prog forge --metadata` 回 `persistent`——同一執行檔不同
子命令各報自己的 lifecycle。

### register 的 import-time 副作用（慣例）

`register*` 系列已設計成純宣告，但**仍建議在 `main()` / `__main__` 區塊內呼叫**，不要放在 module
頂層。理由：頂層 metadata 是 module-global 的單例，若工具 A 在頂層 `register()`、又被工具 B
`import A` 來重用其函式，A 的頂層宣告會在 import 時就寫入全域、與 B 自己的宣告互相覆寫。把
`register()` 留在「確定以腳本身分執行」的 `main()` 內，import 重用時就完全無副作用。

---

## §2 生命週期

### metadata field

| key | 型別 | 允許值 |
|---|---|---|
| `lifecycle` | string | `"one_shot"` \| `"persistent"` |

### 說明

描述程式從啟動到結束的持續模式。

| 值 | 語意 |
|---|---|
| `"one_shot"` | 啟動 → 執行 → 結束。程序存活時間與任務等長 |
| `"persistent"` | 啟動後持續存活，等待請求或持續產出，直到被顯式終止 |

**預設值**：若此 key 不存在、為 `null`、或為 `false`，等同於 `"one_shot"`。只有明確需要 `"persistent"` 時才需宣告。

---

## §3 跨呼叫狀態

### metadata fields

| key | 型別 | 說明 |
|---|---|---|
| `state` | string | `"stateless"` \| `"stateful_external"` |
| `state_dirs` | array | 遵守標準目錄慣例時使用的目錄子集；見 [`composite_spec.md`](../composite_spec/README.md) |

### 說明

描述程式在多次執行之間是否保有、影響或累積狀態。

| 值 | 語意 |
|---|---|
| `"stateless"` | 每次執行互相獨立，輸出完全由當次輸入決定。可安全重試、平行執行。 |
| `"stateful_external"` | 狀態存於程式外部。呼叫方需知悉此程式會對外部狀態產生讀寫副作用。 |

**預設值**：若此 key 不存在、為 `null`、或為 `false`，等同於 `"stateless"`。只有明確需要 `"stateful_external"` 時才需宣告。

> `"stateful_internal"`（狀態存於程序記憶體）理論上存在，但程序結束後記憶體釋放，實務上等同於每次 one-shot 都從外部重載狀態，故不列為獨立值。

### 標準狀態目錄慣例（Terminal 實作）

`state: "stateful_external"` 在 terminal 環境下的標準路徑慣例（`.config`、`.cache`、`.state`、`.data`），以及其對 §1 / §5 / §6 的跨軸影響，定義於 [`composite_spec.md`](../composite_spec/README.md)。

### 與 §2 的關聯

| 組合 | 常見形態 |
|---|---|
| `lifecycle: "one_shot"` + `state: "stateless"` | 最典型的 CLI 工具：給定輸入 → 產出輸出 → 結束（兩者皆為預設，可省略宣告） |
| `lifecycle: "one_shot"` + `state: "stateful_external"` | 資料庫更新腳本、append log 工具：每次執行都改動外部狀態 |
| `lifecycle: "persistent"` + `state: "stateless"` | 無記憶的 stateless server（每個請求獨立處理，無跨請求狀態） |
| `lifecycle: "persistent"` + `state: "stateful_external"` | 有狀態的 server（e.g., session 管理、累積計數器，狀態寫入外部） |

### 對呼叫方的影響

- `"stateless"`：呼叫方可自由重試或並行，無須考慮副作用。
- `"stateful_external"`：呼叫方應確認狀態路徑（由 `--help` 或標準目錄慣例推斷）；並行執行需自行處理競爭條件（race condition）。

### register() 範例

```python
import ai_core

def main():
    ai_core.register(
        lifecycle="one_shot",
        state="stateful_external",
    )

    parser = argparse.ArgumentParser()
    # ...
```

`--metadata` 輸出：

```json
{"lifecycle": "one_shot", "state": "stateful_external"}
```
