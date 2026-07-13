## 中斷恢復慣例（Interruption Recovery Convention）

### 概述

當程式宣告自己在中斷後具備某種恢復能力——§5 `interruptible` 的 `resumable` / `rollback` / `resettable`，或 §6 `guarantee: "transactional"`——這些能力的底層實作機制是同一個：**在 `.state` 留下一份恢復記錄，下一次啟動時偵測該記錄並據以恢復**。

本慣例直接建立在標準狀態目錄慣例之上。上層慣例已定義 `.state` 為「程式目前所在的 stage」；本慣例進一步規定該 stage 內部要存放什麼、新一輪執行如何讀取它、以及據此如何恢復。恢復能力由 §5 / §6 既有欄位宣告，落地形式由本慣例的 on-disk 約定承擔——本慣例不新增 metadata 欄位（見下文）。

### 觸發條件

`state: "stateful_external"` + terminal 環境 + 下列任一：

- `interruptible ∈ {"resumable", "rollback", "resettable"}`
- `guarantee: "transactional"`

### 目錄結構

```
.state/<program_name>/recovery.json    ← 恢復記錄 manifest（標準最小欄位）
.state/<program_name>/recovery/         ← opaque payload（進度資料 / journal，程式自訂，選填）
```

`recovery.json` 是程式間的真正合約；`recovery/` 子目錄存放恢復所需的實際資料，格式由程式自訂。

### 恢復模式

把分散在 §5 / §6 的軸值，統一為「對半完成進度的三種處置」：

| 模式 | 來源軸值 | 偵測到未完成記錄時的行為 | 還原目標 |
|---|---|---|---|
| `resume` | §5 `resumable` | 從斷點接續執行 | 不還原，往前推進 |
| `rollback` | §5 `rollback`、§6 `transactional` | 撤銷所有未提交修改 | 完整還原至呼叫前的原始狀態 |
| `reset` | §5 `resettable` | 重置到某個安全點 | 安全狀態（不保證回到原始） |

> `transactional` 在單次執行內部保證 all-or-nothing；當執行被外部強制終止（kill）繞過保證機制時，下一輪啟動透過 `rollback` 模式補救未提交的殘局。這把 §5 的 `rollback` 與 §6 的 `transactional` 在實作層接了起來——前者描述「中斷後能回滾」的能力，後者描述「執行具事務性」的承諾，兩者共用同一份 journal。

### `recovery.json` 最小欄位

| 欄位 | 型別 | 說明 |
|---|---|---|
| `status` | string | `"in_progress"`（執行中／被中斷）｜ `"done"`（正常完成）|
| `mode` | string | `"resume"` ｜ `"rollback"` ｜ `"reset"`；此程式採用的恢復策略 |
| `ts` | string | 上次寫入此記錄的時間戳 |
| `payload` | string（選填）| 指向 `recovery/` 內進度／journal 檔的相對路徑 |

`status` 與 `mode` 為必填；其餘欄位與 `recovery/` 內 payload 的格式由程式自由擴充。

### 啟動恢復流程（標準演算法）

偵測為 **auto**——程式啟動時自動讀取恢復記錄，呼叫方無需傳入任何 flag。

1. 啟動，讀取 `.state/<program_name>/recovery.json`
2. 記錄不存在，或 `status == "done"` → 視為乾淨開始，正常執行
3. `status == "in_progress"` → 依 `mode`：
   - `resume`：載入 `payload`，從斷點接續
   - `rollback`：依 `payload` 撤銷未提交修改，還原後再正常執行（或退出交由呼叫方重試）
   - `reset`：清除／重置到安全點，再正常執行
4. 開始實際工作前，將 `status` 設為 `"in_progress"`；正常完成後設為 `"done"`（或刪除記錄）

是否額外提供 `--fresh` / `--resume` 之類的覆寫 flag，由程式自行決定，不在本慣例的強制範圍。

### 為何不新增 metadata 欄位

恢復能力已由 §5 `interruptible` 與 §6 `guarantee` 宣告，這正是 hub 排程時真正需要知道的（能否安全重試、能否中斷）。恢復記錄的內部細節屬 runtime 行為，hub 既不需要、也讀不到（要讀得啟動程式）。因此本慣例**不新增頂層 metadata 欄位**——觸發由 `interruptible` / `guarantee` + `state_dirs` 含 `"state"` 隱含，真正的程式間合約是 on-disk 的 `recovery.json`。新增 recovery 欄位只會與 §5 / §6 重疊，違反軸的正交性。

### 跨軸涵蓋

| 軸 | 職責 |
|---|---|
| §3 跨呼叫狀態 | `recovery.json` 與 payload 都是 `.state` 下的外部狀態；`state_dirs` 應包含 `"state"` |
| §5 可中斷性 | `interruptible` 宣告「中斷後能恢復」的能力；本慣例規定「怎麼恢復、記錄在哪」 |
| §6 執行保證 | `guarantee: "transactional"` 的回滾承諾，透過 `rollback` 模式落地 |

### 並發

沿用標準狀態目錄慣例的立場：核心規範不處理多實例並發。若同名程式預期多實例並發，應將恢復記錄以 PID / session 隔離（如 `.state/<program_name>/<pid>/recovery.json`），否則彼此覆蓋。

### 完整範例

一個「可續傳的大檔下載器」：

```python
ai_core.register(
    lifecycle="one_shot",
    state="stateful_external",
    state_dirs=["state", "data"],
    interruptible="resumable",
    guarantee="idempotent",
)
```

被中斷後重跑時，`.state/<name>/recovery.json`：

```json
{
  "status": "in_progress",
  "mode": "resume",
  "ts": "2026-05-23T10:00:00Z",
  "payload": "recovery/offset.json"
}
```

語意解讀：程式重新啟動，自動讀到 `status: "in_progress"` + `mode: "resume"`，從 `payload` 記錄的 offset 接續下載，不重下已完成的部分（`guarantee: "idempotent"` 保證重跑不累積副作用）。完成後 `status` 設為 `"done"`。
