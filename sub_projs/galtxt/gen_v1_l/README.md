# gen_v1_l —— 事實層地基・玩具版（Common Lisp 移植）

← [galtxt INDEX](../INDEX.md)｜原版：[gen_v1/](../gen_v1/README.md)（Lua）｜設計定調：[00_設計.md](../gen_v1/00_設計.md)

> **gen_v1（Lua）的逐行為移植**：同一張帶型別邊的分層 LOD 網、同兩道寫入門、
> 同九個示範同一組 assert。設計文檔不重抄——一切定調以 gen_v1/00_設計.md 為準。

## 跑

```sh
cd gen_v1_l && sbcl --script main.lisp     # SBCL（開發驗證用 2.6.6）；示範 ①～⑨ 全數自帶 assert
```

## 檔案（單檔 ≤120 行慣例，見 [conventions](../workflows/common/conventions.md)）

| 檔 | 是什麼 | 對應 Lua 檔 |
|----|--------|-------------|
| [facts.lisp](facts.lisp) | **事實庫本體**：`db`／`kn` struct、`fact`/`fget`/`fset`；總則註解在此 | facts.lua |
| [facts_store.lisp](facts_store.lisp) | 編譯期門①（db-add）＋ db-modify（canon 鎖）＋知識視圖 db-view ＋ db-dump | facts_store.lua |
| [facts_lod.lisp](facts_lod.lisp) | 編譯期門②（db-refine：節點取點／連結加段／約束滿足→掛起工單） | facts_lod.lua |
| [facts_query.lisp](facts_query.lisp) | 讀取五動詞：db-resolve／db-match／db-trace／db-backrefs／db-layer | facts_query.lua |
| [facts_tx.lisp](facts_tx.lisp) | 執行期門：speculate 交易（db-begin→tx-apply→tx-check→tx-commit/tx-rollback）＋呈現傳染 | facts_tx.lua |
| [facts_util.lisp](facts_util.lisp) | 共用小工具（has-p／佔位判定／排斥邊檢查／expect-error） | facts_util.lua |
| [seed.lisp](seed.lisp) | 建材期素材：河鹿堂定點灌庫，`(seed-db)` 回傳 db | seed.lua |
| [demos_graph.lisp](demos_graph.lisp) | 示範①～④：分層網／節點 LOD／連結 LOD／知識視圖 | demos_graph.lua |
| [demos_gates.lisp](demos_gates.lisp) | 示範⑤～⑨：寫入門／洩底／排斥邊／speculate／約束細化 | demos_gates.lua |
| [main.lisp](main.lisp) | 跑批入口：純 load 腳本（無 package／ASDF），依序 load→seed→九示範 | main.lua |

## 行為對齊聲明

- **九個示範逐條對齊** Lua 版的 assert（①～⑨ 同一組條件、同一組錯誤訊息關鍵字
  「矛盾防線」「排斥邊」「掛起工單」「不可見」），跑完印同款結案句（加註 Common Lisp 版）。
- **確定性鐵則同款**：無 RNG、無 IO 依賴、無時鐘；事實遍歷走插入序向量（fill-pointer vector），
  hash-table 只做 id 點查、絕不遍歷（CL hash-table 遍歷序不確定——對應 Lua `pairs()` 之戒）；
  dump 的知識視圖名單 `sort` 後輸出。已抽驗：跑兩次輸出逐位元相同、換 cwd 跑亦相同。
- **兩道寫入門、六動詞介面、四柱不變式**全部同構；已知簡化清單同 gen_v1/README.md，不重抄。

## CL 特有的取捨

| 議題 | Lua 版 | CL 版的對應 |
|------|--------|-------------|
| 自由欄位的事實 | table（`f.mood` 隨手加） | 每筆事實一個 eq hash-table＋`fact`/`fget`/`fset`；缺欄回 `nil`（≡ Lua 缺鍵） |
| `nil` vs `false` | `canon=false` 起手 | CL 無 false，`canon` 缺欄即 `nil`＝未呈現；dump 印 `NIL`/`T`（快照只求自身確定性，不求與 Lua 逐字同文） |
| 多值回傳 | `return nil, why` | `(values nil why)`＋呼叫端 `multiple-value-bind`（resolve／check／refine 掛起） |
| 閉包物件 `tx.apply(...)` | 交易＝閉包 table | `tx` struct＋`tx-apply`/`tx-check`/`tx-commit`/`tx-rollback` 普通函式，語義一對一 |
| 就地改陣列 | `table.remove` 就地 | `remove-val` 函數式回新 list、`setf` 回 struct 槽位（等效，且不共享底層） |
| 模組組織 | `require` 各回一 table | 純 load 腳本（main.lisp 定序）；不上 package／ASDF——玩具版取最簡單能跑 |
| 欄位名 | `edge_type` | keyword `:edge-type`（CL 命名慣例）；字串值與事實 ID 照抄中文 |
| `pcall` 測擋下 | `pcall`→`assert(not ok)` | `expect-error` 宏（`handler-case` 包一層） |
