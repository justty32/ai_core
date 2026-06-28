# core_handy — ac_helper（ai_core 的 C++ 濃縮線）

依託九軸標準、輔助作者寫出**自動符合標準、會正確回應 `--metadata`** 的 shell 程式。
是 `src/ai_core/_core.py`（`register()`/`intercept()`）的 C++ 對應；與 Python 線刻意分開。

## ★ 事實基準 vs 歷史（2026-06-28 收斂）

查「現在長怎樣」只看**事實基準**；查「為什麼這樣定」才去翻歷史。

| 角色 | 位置 | 說明 |
|---|---|---|
| **事實基準・描述面** | [`defs/axes.hpp`](defs/axes.hpp) | 九軸描述性 metadata 型別。**唯一可信現況。** |
| **事實基準・設施面** | [`impl/*.hpp`](impl/) | 替程式做掉某事的設施碼（I/O、state、transaction、cli、meta_json…）。 |
| 膠水 / 示範 | `ac_helper.hpp`、`main.cpp` | 對外進入點與示範。 |
| **歷史討論記錄** | [`notes/`](notes/) | 設計如何走到定案的 WHY。各軸只留最終拍板；舊輪次見 `notes/archived/`。**衝突時以 `defs/`、`impl/` 為準。** |
| 封存 | [`notes/archived/`](notes/archived/) | 被 supersede 的舊輪次 + 已結案的 `defs_review/`。非現況。 |

> 收斂前的舊認知（「notes 是事實」）已作廢。事實基準 = `defs/` + `impl/` 的程式碼；notes 為歷史。

## 九軸描述面現況（= `defs/axes.hpp`）

| 軸 | 型別 | 軸 | 型別 |
|---|---|---|---|
| 1 entries | `Entry{direction, content}` | 6 interruptible | `unsigned level`（名目碼） |
| 2 lifecycle | `bool persistent` | 7 guarantee | `enum {none,idempotent,transactional}` |
| 3 state | `bool stateful` | 8 dry_run | `bool allow_dry_run` |
| 4 state_dirs | 純設施（描述面外包軸 3）→ `impl/state.hpp` | 9 nondeterministic | `unsigned uncertainty` |
| 5 resources | `Memory/Gpu/Time/Network` + disk | | |

> ★ **單一 extra（2026-06-28 決定）**：各軸不再各帶 extra；全軸的低保真補充收斂成**唯一一個 `Meta::extra`**
> （`optional<map<string,string>>`）。序列化為頂層 `"extra":{...}`（有值才出）。原各軸 PARKED 的東西
> （軸 1 transport/mode、軸 9 治理證書 model/test_set/stability…）一律落這個袋，靠 key 命名自辨歸屬。

## 建置

```bash
cmake -G "MinGW Makefiles" -S . -B build && cmake --build build   # 見 CMakeLists.txt
```
