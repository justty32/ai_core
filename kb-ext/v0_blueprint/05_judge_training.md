# 05 · 零錯誤函數訓練（固化引擎最小實例）

> 定案＝如何「訓練」一個只吐 true/false、零錯誤的函數（給 I/O pairs 把函數凹到全綠）。過程：`discussion_logs/judge_round_2_*`。**尚未登記進 roadmap/DECISIONS，但提出第九軸 `coverage` 欄位的增補建議。**

## 一句話

「訓練一個零錯誤 true/false 函數」＝把含 LLM 留白的函數固化成**三層三明治**，直到一組 I/O pairs（test_set）全綠。**「零錯誤」精確地說＝ `nondeterministic:{test_set, stability:100%}`——第九軸證書本身。** 這是 roadmap §3.6 固化引擎 + 證書 + 飛輪 的第一個可操作實例。

## 三層三明治（四人獨立收斂到同一結構）

```
input → 層1 規則 rules（確定性、泛化、聰明模型歸納 f(x)->bool）  miss→sentinel↓
      → 層2 override 表（memoize 死記殘差，機械保證 train 零錯誤）  miss→sentinel↓
      → 層3 LLM fallback（唯一非確定/未認證/帶證書的點）
      → is_true_or_false 邊界門 → 嚴格 true/false
```
用 `with_fallback`/`route` 串，全用現成積木（memoize+compose+bind），不造新輪子。三層映射 roadmap：層1＝已固化確定性碼、層2＝固化半成品、層3＝模糊前沿。

## 五個核心洞察

1. **零錯誤＝證書本身**（C1）：是 stability=100% **on THIS test_set**，不是形上學零錯誤。證書價值不在「100%」，在它逼你寫下 test_set 是什麼。
2. **train 零錯誤 trivial 且危險**（C2）：memoize 查表是解析解，「train 全綠」信息量是零，分不出「學會」vs「背下來」。
3. **override 表大小＝過擬合溫度計**（C3，AIRE 支點）：`|override|/|train|` ＝死記佔比，一個訓練時可數出的整數。**把奧坎剃刀做成 metric。**
4. **硬零錯誤不必犧牲泛化**（C4）：規則層管泛化（容許 train 漏判）、override 兜住所有漏判→整體無條件達 train 硬零錯誤，代價只在 |override| 大小。
5. **holdout 是唯一裁判**（C5）：train 上查表與規則都 100%；只有 held-out 照出「查表 holdout≈瞎猜 vs 規則 holdout 仍高」。

## unknown 移位（三人同構）

對外嚴格二元（使用者贏），unknown 沒消失而是移位：SSE 的內部 sentinel / SSA 的「飛輪還沒轉到的座標」/ SGA 的「集外未認證區」。純二元（輸出契約，集內成立）與不信任（邊界契約，集外接管）是兩層契約，不矛盾。

## 「訓練」＝元函數 + 第九軸躍遷

`train: (skeleton, pairs, policy) → forged_judge 資產`——收在代數內的資產工廠，本身也是可被 hub 掃描的函式。躍遷：`nondeterministic: true`（骨架）→ `{model, test_set, stability:1.0, coverage, holdout}`（資產）；`guarantee:idempotent ⟺ LLM fallback 層已死`。固化後 judge 降階成確定原子，甚至當下一個 train 的 validate（**判斷器訓練判斷器**）。

## 規範增補建議

**第九軸證書應加 `coverage` 欄位**（規則層命中比例）——查表與規則在 test_set 上都 stability=1.0 但可信度天差地別，**證書光記 stability 會撒謊**。coverage 高＝真泛化可降階成原子；低＝藏起的查表、不該在飛輪裡降階。待定奪是否扶正進 `core_nature/ axis_spec.md §9`。

## 對 roadmap §3.6 的答案

固化引擎手動 vs 自動 → **半自動**：聰明模型提案規則（自動）+ 失敗迴圈機械驗收（自動）+ 人/聰明模型看哪些殘差該歸納（半手動）。固化梯度可測：`|override|↓`、`coverage↑`、`unknown 比例↓`。
