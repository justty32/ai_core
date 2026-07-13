# 軸 5：`resources`（資源特性）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本檔只留最終拍板（Round 3）與現況脈絡；被取代的 Round 1–2 見 [`archived/axis_5_history.md`](archived/axis_5_history.md)。

> 狀態：✅ 定案（Round 3，砍 cpu，supersede R2）。第一個「自由 key-value 袋」本質的軸 → 三層通則首次完整套用。
> 第一個「無最小必須、純推薦+額外」的軸。R3 移除 cpu（唯一消費者是 hub）、回歸權威預定義集。
> **2026-06-28 再收斂：extra 已上收為單一 `../defs/axes.hpp` 的 `Meta::extra`；本軸 `Resources` 移除 extra 欄位（自定義依賴/cpu 改走 Meta::extra）。**

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`resources` 僅驗證為 dict，自由 key-value）、
`docs/spec/lib_spec.md §4`、`docs/spec/axis_spec.md §4`。

`resources` ＝ 對**計算資源的消耗／佔用（含時間）**的宣告，供 Function Hub 排程決策。
自由 key-value：**預定義 key 有規範 value 格式**（hub 可解析）；**自定義 key** value 不限（hub 讀得到、未必懂）。

**資源生命週期維度**（axis_spec §4 Phase 1）：種類 / 啟動時申請 / 執行中動態 / 結束後釋放 / 持續佔用(idle)。

### 預定義 key（lib_spec §4）

| key | 字串形式 | 物件形式 | 備註 |
|---|---|---|---|
| `memory` | `"4gb"`（＝峰值） | `{startup, peak, idle}` 皆選填 | `idle` 僅 `persistent` 有意義 |
| `gpu` | `true`（需 GPU） | `{vram: "6gb"}` | |
| `time` | `"30s"`（預期） | `{expected, max}` | |
| `disk` | `"500mb"` | — | 執行期暫用、結束清除；持久資料歸軸 4 `data` |
| `network` | `true` | — | 是否需網路 |

- 單位：容量 `b/kb/mb/gb`、時間 `ms/s/m/h`。
- 自定義 key 範例：`llm_entry: true`、`db: {type: postgres}`、`render_server: {port: 7860}`。
- **CPU**：Phase 1 分析提到，但**無預定義 key** → 要表達就走自定義（extra）。
- **預設**：缺席＝無任何資源宣告。

---

## 開放問題 / 後續輪次

- 設施面（rate-meter/限流/配額）API ——待 impl 集中重做設計（network.traffic / cpu.cores 是其輸入）。
- `idle` 僅對 persistent 有意義，是否要型別層強制（跨軸 2）——傾向不強制，文件約束即可。

---

## Round 3（✅ 砍 cpu，supersede R2，defs 重新思考 2026-06-27）

對照 `defs_review/axis_5_resources.md`，套「別管 hub」原則（2026-06-27 使用者定）。

### 唯一改動：移除 `cpu` opt 欄位

`cpu` 是 R2 刻意偏離權威新增的（權威無 CPU 預定義 key）。它的存在理由 100% 是 hub 排程——
筆記原文：「resources 受眾是 hub 排程器，它要回答 OS 不管的『能吃多少平行度』以**避免超賣**」。
而：
- OS 排程器本就把 CPU 抽象掉（不宣告也能跑）→ 對「能否執行」無意義；
- roadmap 的 consume-rate 管 token/錢/GPU，**不含 CPU 核心數** → 對 impl rate-meter 也無意義。

cpu 唯一消費者是 hub。套「別管 hub」→ **砍掉，回歸權威**。順帶消滅 review ②的誤填顧慮與偏離成本。
真要表達內部平行度的人，走 `extra`。

### 未改動（review 其餘兩條）

- **① 值無量綱**：review 自承「唯一受眾是 hub」→ 純 hub 動機。維持自由字串（沿軸 1「先當 string」），不正規化。
- **③ idle × persistent**：one_shot 填 idle 只是無意義、不危險。維持文件約束，不把 memory struct 綁死 lifecycle。

### 保留的預定義 key（餵 impl consume-rate，非 hub，全留）

`memory` / `gpu`(vram) / `time` / `disk` / `network`(traffic)。其中 network 維持 R2 的 struct 升格
（traffic 餵 consume-rate 計量，是 roadmap 核心、非 hub）。

### 型別草圖（定案）

```cpp
struct Memory  { std::optional<std::string> startup, peak, idle; };  // idle 僅 persistent
struct Gpu     { std::optional<std::string> vram; };
struct Time    { std::optional<std::string> expected, max; };
struct Network { std::optional<std::string> traffic; };

struct Resources {
  std::optional<Memory>      memory;
  std::optional<Gpu>         gpu;
  std::optional<Time>        time;
  std::optional<std::string> disk;       // "500mb"
  std::optional<Network>     network;
  // 自定義依賴（llm_entry/db/render_server…）與 cpu 改走 `Meta::extra`（已上收，本軸不再帶 extra）
};
```

> 與權威的分歧（記明）：R2 的兩處分歧（新增 cpu、network 升 struct），R3 撤掉前者、保留後者。
> network 升格的理由是 consume-rate（非 hub），站得住；cpu 的理由是 hub，倒。
