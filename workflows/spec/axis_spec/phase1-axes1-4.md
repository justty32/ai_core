## Phase 1 初步觀察：從使用者體驗出發

> 粗糙想法紀錄，尚未對應到各軸。

使用者體驗的自然演進順序：

### 1. One-shot（起點）

最常見的形式。執行一次，拿結果，結束。

### 2. Multi-shot（事情變複雜）

需要多次執行，且每次執行的結果不同（有跨呼叫的狀態）。狀態的儲存位置決定了程式的形態：

- **狀態存外部**（檔案、DB 等）：等於每次執行的輸入形式改變了（`--state-file` 之類），程式本身仍可以是無狀態的
- **狀態存內部**：等於修改程式自身——這幾乎不實際，程序結束後記憶體釋放，只有把狀態寫進 code/config 才算「存內部」，但那本質上還是外部檔案

> 推論：真正「stateful-internal」的情況極少；大多數 multi-shot 實際上是 stateful-external 搭配不同輸入。

### 3. Persistent（一直待在那邊）

執行後不結束，持續存活。依用途細分：

| 子類型 | 行為 | 例子 |
|---|---|---|
| Monitoring / Streaming | 持續往 stdout 吐資料 | log tail、sensor reader |
| Resource entry / Scheduler | 管理資源，做請求排程 | LLM Entry Manager、job queue |
| Periodic / Wake | 定期觸發某動作，其餘時間休眠 | cron-style daemon |
| Server（可互動） | 等待請求，每次請求回應一次 | HTTP server、RPC server |

### 4. Server + Client 配對

Server 模式通常伴隨一個 client 程式。兩者之間的通訊方式形成一個獨立關切點：

- stdin / stdout（最簡單，適合本機同步）
- Shared memory（本機高速）
- File I/O（非同步、可持久）
- TCP / Unix socket
- HTTP API / RPC

> 通訊方式是 §1 I/O 型態 與 §8 環境模式 的交叉點；server 本身的生命週期屬於 §2。

---

## §1 I/O 型態

<!-- 要描述的問題：輸入從哪來、輸出往哪去、是否需要執行途中的互動 -->
<!-- 已知形式：無輸入、stdin、互動式（中途需 stdin）、streaming push、streaming pull -->

### Phase 1 分析

此軸應拆為兩個獨立子軸，兩者正交：

**§1a 通訊出入口**：資料透過什麼媒介進出程式

| 出入口 | 說明 |
|---|---|
| stdio / stdout / stderr | 最原始的 CLI 介面 |
| File I/O | 以路徑指定，非同步、可持久 |
| TCP / Unix socket | 跨程序或跨機器 |
| HTTP / RPC | 有協議語意的網路通訊 |
| Shared memory | 同機器高速交換 |

**§1b 資料進出形式**：資料是一次性交換還是持續性流動

| 形式 | 說明 |
|---|---|
| 一次性（batch） | 輸入一次讀完，輸出一次寫出 |
| Streaming（push） | 輸出方持續推送，接收方逐筆消費 |
| Streaming（pull / generator） | 接收方主動拉取，輸出方按需產生 |
| 互動式 | 執行途中需要使用者介入輸入（stdin prompt） |

> §1a 決定「用什麼管道」，§1b 決定「資料怎麼流」。Server+client 的通訊方式是 §1a 的子集；streaming 是 §1b 的子集。



---

## §2 生命週期

<!-- 要描述的問題：執行單元從啟動到結束的持續模式 -->
<!-- 已知形式：one-shot、persistent、lazy/warm、detached -->

### Phase 1 分析

生命週期只有兩個根本形態：

| 形態 | 說明 |
|---|---|
| **One-shot** | 啟動 → 執行 → 結束。程序存活時間與任務等長 |
| **Persistent** | 啟動後持續存活，等待請求或持續產出，直到被顯式終止 |

其他之前列出的形式都是這兩者的子環節或組合：
- **Lazy / Warm**：Persistent 的啟動策略（第一次請求才真正啟動，閒置後可選擇結束）
- **Detached / Fire-and-forget**：One-shot 的排程策略（呼叫方不等待結果）
- **Periodic / daemon**：Persistent + 定期觸發內部動作
- **Streaming**：Persistent 的輸出模式，或 One-shot 的 §1b 形式

> 生命週期軸的值只有兩個；其他描述應歸到對應的軸。



---

## §3 跨呼叫狀態

<!-- 要描述的問題：單次執行以外是否保有、影響或累積狀態 -->
<!-- 已知形式：stateless、stateful-external、stateful-internal -->

### Phase 1 分析

**Multi-shot 是這個軸的衍生描述**：multi-shot 只是說「需要多次執行且狀態跨次累積」，狀態本身的儲存位置才是根本問題。

| 分類 | 說明 |
|---|---|
| **Stateless** | 每次執行互相獨立，輸出只由輸入決定 |
| **Stateful-external** | 狀態存於外部（檔案、DB、環境變數），程式透過參數或約定路徑讀寫 |
| **Stateful-internal** | 理論上是「修改程式自身」的另一種說法——程序結束後記憶體釋放，所以「存內部」幾乎等同於 stateful-external（把狀態寫進 config / state 檔）。此分類實務上可視為不存在。 |

> Stateful-external 與 §1a 通訊出入口有重疊：狀態存在哪裡，就是用那個出入口。此軸著重的是「有沒有跨呼叫的副作用」，而非「怎麼傳輸」。



---

## §4 資源特性

<!-- 要描述的問題：對計算資源的消耗或佔用（含時間） -->
<!-- 已知內容：執行時間、記憶體、CPU、GPU；對應 --metadata 的聲明 -->

### Phase 1 分析

資源描述需要涵蓋**資源的生命週期**，而不只是「大約用多少」：

| 描述維度 | 問題 |
|---|---|
| **種類** | 用了哪些資源？（CPU、記憶體、GPU、磁碟、網路、時間） |
| **啟動時申請** | 執行一開始就占用哪些？（e.g., 模型載入後常駐記憶體） |
| **執行中動態** | 途中會額外申請或釋放哪些？（e.g., batch 處理時記憶體峰值） |
| **結束後釋放** | 結束後會釋放哪些？哪些殘留？（e.g., temp files、port binding） |
| **持續佔用** | Persistent 程式持續佔用的底線資源（idle footprint） |

這些全部反映在 `--metadata` 的回傳中，是 Function Hub 做排程決策的依據。
