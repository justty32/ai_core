# Round 3: 終局批准（Ratification）

> 目的：四位專家對 Round 2 主持人收斂的三題裁決表態，並給最後異議。**結果：全員放行，附三條必補的 schema 條款。** 補完即凍結，V0 可開工。

---

## 四位專家終局表態（摘要）

### SSE — Q2 拍板：接受「hub 早攔」，撤回「sfc 報錯」
anchor 定位是純文字搜尋、零隨機的確定性操作，按治理原則「拒絕為預設」，能在 LLM 呼叫前用確定性程式碼擋掉的就必須擋。「省一次呼叫」只是附帶好處，真正的點是 **locate 是 hub 編排層職責，不是 sfc 的**——sfc 是函式分發器，不該背定位語意。收口細節：`locate` 由 hub 組 target 時做，命中寫 `target.line`、未命中寫 `null` + `trace[locate].ok=false, reason="anchor_not_found"` 並直接回 `rejected`、不進笨模型。Q1/Q3 無異議背書。**判定：可開工、無阻擋點。** 六檔依賴樹狀無環，第一個建議寫 **`asset.py`**（其餘檔的共同地基），次序 `asset → trace → skeleton → isolation → line_assistant → demo`。

### SSA — schema 凍結前唯一要求：把 append-only 從「精神」變「條文」
把 `lifespan/degradation/safety/certificate` 塞進同一 envelope **不違背** append-only——鐵律不是「envelope 不准長欄位」，而是「**沒有任何一站可以改寫上游已寫進去的東西**」。**凍結前唯一要補的約束**：schema 須明文註記「頂層欄位除 `trace_id`/`lifespan` 等建立時即固定的識別/分類欄外，皆不可後寫；一切站間輸出走 `trace[]` append」。Q2「hub 早攔」不算越權——anchor 在不在是「能不能進下一站」的**轉運準入判斷**，正是轉運站該做的純結構檢查（hub 不去「解讀」內容做決策才是底線）。**判定：補上那條約束即可凍結、V0 可開工。**

### SGA — Q3 拍板：status 凍三值，但 `rejected` 必須帶 `reason`
接受 `uncertified / syntax_ok / rejected`。`rejected` 是**終態、不可重試**（retry 發生在 `rejected` 之前、在 `uncertified` 內部循環；寫上 `rejected` 即吊照，不准退回 `uncertified`）。**但「護欄 fail-closed」與「retry 耗盡」在 status 上不分、在 reason 上必分**——撤照意義不同：retry 耗盡是「笨模型能力不足」(asset 無害、可換模型重試)，fail-closed 是「LLM 試圖碰安全節點」(asset 帶毒、**永久汙點不可洗白**)。故 `rejected` 須同時寫 `reason ∈ {guardrail_violation, retry_exhausted}`。三層護欄 **A/B/C 一個都不能砍**（各擋一類攻擊面、純 AST walk 零相依）。`SENSITIVE_NAMES` V0 硬編碼進 `lib/skeleton.py` **接受**，條件：(1) 它是黑名單種子非白名單，漏網由 A 保留清單 + C diff 兜底；(2) 須為模組頂層單一常數、可被 import 檢視，不散在函式裡。**判定：補 `reason` + 三層全做 + 種子頂層常數 → 條件放行、可開工。**

### AIRE — Q1 拍板：兩列 pivot 夠用，但需補配對主鍵 `task_id`
接受凍結 `raw`/`ast_skeleton` 兩值。pivot 的統計力在**行維度（tier）非列維度**；要證的命題是「同一笨模型，餵 ast_skeleton 比餵 raw 填碼正確率顯著更高」——raw（對照組）vs ast_skeleton（處理組）的兩臂比較，兩列正是最小完備形式。多加 strategy 是 v0.x 的二階優化題，會稀釋 V0 的一階命題。`len(s.split())` 粗估 token **接受**——ratio 是相對量，split 對 raw/skeleton 施加同向系統偏差，相對關係保留（紀律：ratio 只做同源比較，不拿絕對 token 對接 entry manager 計費）。**唯一點名補一個採集點：`task_id`**（同一 anchor 任務在 raw 與 ast_skeleton 兩臂下要能配對，才能做「同一任務 raw 失敗但 skeleton 成功」的配對比較，統計力高一量級、且能直接展示飛輪最有說服力的證據）；每格 N≥30 樣本。**判定：補 `task_id` → 放行、可開工。**

---

## 主持人最終裁定：schema 凍結（含三條補丁）

三題裁決全員背書，無人推翻。納入三條必補條款後，**ATP v0 schema 正式凍結**：

| 補丁 | 提出 | 併入內容 |
|---|---|---|
| **P1 append-only 條文化** | SSA | schema 頂部註明：頂層欄位除 `asset_id`/`trace_id`/`kind`/`schema_version`/`lifespan` 等**建立時即固定的識別/分類欄**外皆不可後寫；一切站間輸出（含 certificate/degradation/safety 的結果）以對應站的 `trace[]` entry 為權威來源，頂層鏡像欄只反映最後一次 append、不得被獨立改寫。 |
| **P2 `rejected.reason`** | SGA | `certificate` 增 `reason` 欄（僅 `status==rejected` 時非 null），凍結兩值 `guardrail_violation`（安全否決，永久終態）/ `retry_exhausted`（能力不足）。`rejected` 為終態不可重試；retry 在 `uncertified` 內部循環。 |
| **P3 `task_id`** | AIRE | asset 頂層增 `task_id`（識別欄、建立時固定）：同一 anchor 編輯任務在 raw 與 ast_skeleton 兩臂共用同值，供 benchmark 配對比較。pivot 每格樣本 N≥30。 |

**三題最終裁決（凍結）**：
- **Q1** `pruning.strategy ∈ {raw, ast_skeleton}`，token 用 `split` 粗估（僅同源相對比較）。
- **Q2** anchor 找不到 → **hub 早攔**，落 `rejected` + `reason`（locate 失敗對應 `retry_exhausted` 類，或可細分 `anchor_not_found`），不送笨模型。
- **Q3** `certificate.status ∈ {uncertified, syntax_ok, rejected}` + `reason`；`approved` 留 v1。

**護欄與安全（凍結）**：三層護欄 A/B/C 全實作；`SENSITIVE_NAMES` 為 `lib/skeleton.py` 頂層單一常數（黑名單種子）；裁掉受保護節點 → `safety.violations` 非空 → `rejected/guardrail_violation`，benchmark 判失敗（安全凌駕語法）。

**開工次序（採 SSE/SSA 共識）**：`asset.py → trace.py → skeleton.py → isolation.py → line_assistant.py → demos/v0_pipeline.py`。

---

## 圓桌會議結論

V0 切片定義經三輪收斂完成、四位專家一致放行。**ATP v0 schema 凍結**，待開工。本輪未動任何 `src/ai_core/_core.py` 或 `core_nature/` 規範——全部產出為 `kb-ext/` 的設計收斂，開工時才落地成 `try_implement/` code。

*產出方式：三輪發言均由內部並行 subagent 獨立扮演四位專家產生（gemini cli 因 Google 停用免費 tier 不可用）；收斂與裁定由主持人（Claude）撰寫。*
