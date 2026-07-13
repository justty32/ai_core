## §4 函式形式在 terminal 中的對應

### 4.1 One-shot

**定義**：執行一次、產生輸出、結束。無狀態，無副作用（除了明確的輸出）。

Terminal 表現：
- stdin → process → stdout / stderr
- exit code 0 = 成功，非 0 = 失敗
- 大多數 Unix 工具屬此類：`grep`、`sed`、`awk`、`jq`...

One-shot 是最基本的形式，也是 pipeline 組合的基礎單元：

```bash
cat file.txt | grep "error" | wc -l
```

每個節點都是獨立的 one-shot process，stdout 自動接到下一個的 stdin。

---

### 4.2 Multi-shot

**定義**：跨多次呼叫保有狀態，本次執行結果影響下次。

Terminal **沒有原生支援**。Multi-shot 在 terminal 中必須拆解為：

> **one-shot + 外部狀態管理**

狀態的持久化策略（由工具自行定義）：

| 策略 | 做法 | 備註 |
|---|---|---|
| 狀態檔 | 每次呼叫讀入 `--state-file path`，執行後寫回 | 最通用，明確可見 |
| 固定路徑慣例 | 工具固定讀寫 `~/.tool/state` 或 `./.tool_state` | 隱式，呼叫者無需指定 |
| 輸出即狀態 | 上次 stdout 直接作為下次 stdin 輸入（無副作用） | 最純粹，但狀態大時不方便 |

**重點**：工具的 `--metadata` 必須聲明它是 multi-shot，並說明狀態的存放位置與格式，否則呼叫者無從得知如何正確使用。

**狀態所有權的兩種子型：**

| 子型 | 說明 | 呼叫者介面 |
|---|---|---|
| Caller-managed | 呼叫者保管狀態，顯式傳入 `--state-file path` | 呼叫者對狀態有完整控制，也需自行處理初始化與清除 |
| Self-managed | 函數自行決定狀態存放路徑（固定路徑慣例） | 呼叫者直接呼叫，無需傳入狀態參數；狀態對呼叫者透明 |

兩者行為語意相同（跨呼叫保有狀態），差別只在**誰持有狀態的所有權**。

---

### 4.3 Persistent

**定義**：長期存活，持續等待呼叫，不主動結束。

Terminal 有兩種子型：

#### 子型 A：互動式（Interactive / REPL）

從 stdin 讀取輸入，處理後輸出到 stdout，循環直到收到結束訊號（EOF 或特定指令）：

```bash
python3        # REPL
sqlite3 db     # 互動式 SQL
```

通訊介面：stdin / stdout（與 one-shot 相同，但 process 不結束）

#### 子型 B：Server / Daemon

在背景持續運行，透過 IPC 機制等待請求：

| IPC 機制 | 說明 |
|---|---|
| Unix socket | 本機 IPC，效能高，路徑即位址 |
| TCP port | 可跨機器，HTTP/JSON-RPC 常用此 |
| Named pipe（FIFO） | 單向，較少用於雙向通訊 |

啟動方式：`&` 置於背景，或由 systemd / init 管理。

**結論**：Persistent 程式建議設計成 server（子型 B），而非互動式。原因：server 可被程式化呼叫，互動式只能給人用。

---

### 4.4 Singleton

**定義**：資源受限，系統中同時只能存在一個（或有限數量的）實例。

Singleton 不是一種「執行形式」，而是一種**資源約束**，可疊加在 persistent 上（最常見）。

Terminal 的實作慣例：

| 機制 | 做法 |
|---|---|
| PID 檔 | 啟動時寫 PID 到 `~/.tool/tool.pid`，啟動前先檢查是否已有 running 實例 |
| Lock 檔 | `flock` 取得獨佔鎖，後來的啟動嘗試直接失敗或等待 |
| OS 服務管理 | 交給 systemd，由 OS 保證單例 |

**典型案例**：LLM Entry Manager——本地 GPU 是有限資源，不允許多個實例同時搶占，因此設計為 singleton persistent server。

---

### 4.5 Streaming

One-shot 的輸出變體：執行期間**邊算邊輸出**，而非等全部完成才輸出。

Terminal 表現：stdout flush 不緩衝（Python 用 `-u` 或 `sys.stdout.flush()`，或 `unbuffer`）。

從 process lifecycle 角度仍是 one-shot（執行完就結束），但輸出是增量的。典型例子：`tail -f`、ping、串流 LLM 回應。

與 persistent 的區別：persistent 等待多個獨立呼叫，streaming 是**單次呼叫**內產生連續輸出。

**變體：Pull-based / Generator 模型**：上述為 push-based（函數主動推送輸出）。Pull-based 反過來——由 caller 主動拉取一個值，函數在兩次拉取之間暫停。Python generator（`yield`）是典型例子。在 terminal 層幾乎不可見（shell 管道預設 push），但在程式內部函數中是獨立的概念。

---

### 4.6 Lazy / Warm

介於 one-shot 與 persistent 之間的中間態：**第一次呼叫才啟動，之後在一段時間內保持活躍等待下一次呼叫。**

| 屬性 | 說明 |
|---|---|
| 啟動方式 | 第一次呼叫時才初始化（非預先常駐） |
| 存活策略 | 呼叫後保持一段 idle 時間（timeout 後自動關閉） |
| 適用場景 | 啟動代價高、但呼叫頻率不固定的工具（如 LLM inference runtime） |

可視為 persistent 的子類（「不預先常駐的 persistent」），或獨立分類——設計上待定。

---

### 4.7 Detached / Fire-and-Forget（分離式 / 射後不理）

**定義**：被呼叫後，立即啟動一個背景任務（或將任務交接給系統守護行程），主行程瞬間結束並回傳狀態，不等待實際任務運算完成。

Terminal 表現：`nohup command &`、`disown`，或不帶 `--wait` 的 `systemctl start <service>`。

與 One-shot 的差異：One-shot 會阻塞呼叫者直到任務完成並輸出結果；Detached 立刻釋放控制權，實際任務的輸出通常導向 log 檔、`/dev/null`，或透過 webhook 異步回傳。

適用場景：觸發耗時極長的非同步任務（大規模資料遷移、觸發 CI/CD pipeline）。

**變體——Async with Result Retrieval（非同步帶回傳）**：主行程立即結束並回傳 job ID，呼叫者之後透過輪詢（polling）或回調（callback）取回結果。純 Fire-and-Forget 是呼叫者不關心結果；此變體是呼叫者**延遲取回**結果。

---

### 4.8 Fan-out / Fan-in（散佈與收集）

**定義**：複合型執行形式。主行程作為協調者（Coordinator），將任務拆解並同時啟動多個子任務（Fan-out），等待所有子任務完成後，將結果聚合（Fan-in）再統一輸出。

Terminal 表現：`GNU parallel`、`xargs -P`、`make -j`。

與 One-shot pipeline 的差異：標準 pipeline（`A | B | C`）是線性串流；Fan-out / Fan-in 是樹狀的平行展開與收斂，本質是為了最大化利用多核資源。

---

### 4.9 Idempotent（冪等式）

**定義**：無論執行一次還是連續執行一百次，對系統狀態造成的改變和最終結果都完全相同。可視為「目標狀態導向」的特殊 One-shot：若系統已達目標狀態，則不產生任何副作用操作。

Terminal 表現：`mkdir -p /path`、`rm -f file`、`rsync`，以及幾乎所有 IaC 工具（Terraform、Ansible）。

與 Multi-shot 的差異：Multi-shot 的狀態通常是「累加」或「推進」的（如對話歷史）；Idempotent 是確保狀態**收斂**到預期的絕對基準線上。

---
