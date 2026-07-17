# uni —— 統一節點 ＋ 共用 search kernel（Common Lisp）

← [galtxt INDEX](../INDEX.md)｜表示源：[uniform_probe/](../uniform_probe/README.md)｜演算法源：[一台搜尋機.md](../一台搜尋機.md)

> **兩面合一的落地**：把 [uniform_probe](../uniform_probe/README.md) 綜合出的**統一節點表示**（一種節點跑所有粒度）
> 與 [一台搜尋機](../一台搜尋機.md) 的**共用 search kernel**（gen_v0 引擎 ≅ director）接成一個能跑的系統。
> 河堤場景：`八股→段→句式→填充→原子事實` 全是同一 `(node…)|(fact…)`；`direct` 搜 `*env*` 生台詞。
> 取代 [realizer_l](../realizer_l/README.md) 的四表四接縫（`*scene*/*tmpl*/*fill*/*變項動詞*` → 一張 `*ns*`）。

## 跑

```sh
cd uni && sbcl --script main.lisp     # load kernel→scene→search，九 demo 全自帶 assert
```

## 檔案（單檔 ≤150）

| 檔 | 行 | 是什麼 |
|----|----|--------|
| [kernel.lisp](kernel.lisp) | 41 | **統一節點 kernel**：`node`/`fact` 兩 macro＋part 折疊（字面/引用/`(? env選)`/`(! 邏輯)`）＋湧現粒度 `層`／`真值?`／`懸空?` |
| [scene.lisp](scene.lisp) | 42 | 河堤場景：八股→段→句式→填充→原子事實，全同一種節點 |
| [search_kernel.lisp](search_kernel.lisp) | 24 | **一支參數化 `search*`**（DFS＋下界剪枝＋狀態穿線）：gen_v0 fill_sequence 與 director 收斂於此 |
| [search.lisp](search.lisp) | 42 | 兩種耦合都建在 `search*` 上：`direct`（可分・搜風格軸）＋`搜耦合`（耦合・全場預算） |
| [main.lisp](main.lisp) | 62 | load 定序＋十 demo |

## 兩面如何合一

- **表示**（kernel＋scene）：一張 `*ns*` 表；節點是 `(? key …)`（env 選）＋`(! …)`（邏輯）＋子引用折成的 realize。粒度由 refs 湧現、不限四種；`真值?` 分事實/字面兩域；懸空引用＝佔位。
- **演算法**（search）：`*env*`（母題/情緒/選路）**就是搜尋維度**——`(? key …)` 每個選擇點都是待搜的 env 值。`direct`＝枚舉 `*axes*` 笛卡爾積、護欄剪枝（非 canon 不可行）、`prior＋icost(intent)＋history` 取最小。
- **接縫一句話**：**一棵統一節點樹 ＋ 一支 `search*`。** gen_v0＝搜 act 粒度的 env、本 director＝搜句式/填充粒度的 env——同一台機器。
- **最後一里已閉合**：`direct`（可分・prior 累加＋終端護欄）與 `搜耦合`（耦合・step 穿線全場預算＋剪枝）**跑同一支 [`search*`](search_kernel.lisp)**——demo⑩ 示範全場「……」預算 ≤cap 這種跨節點約束（flat enum 表達不了、gen_v0 需 DFS），與 director 共用同一函式。[一台搜尋機](../一台搜尋機.md) 的「同一台機器兩種耦合」由此在程式碼層成真、不再是兩份程式碼。

## 九個 demo

表示：① 一種節點跑整場｜② 粒度湧現 0/1/2/3｜③ 真值域 vs 字面域同表分域。
演算法：④ 搜最優（無 intent）｜⑤ intent 甜 vs 靜搜出不同風格｜⑥ 護欄剪枝（含『咖啡廳』的場成本=nil 被排除）｜⑦ `direct-n` 自動生 3 種台詞流。
結構：⑧ 分支＝普通節點（選路由 env）｜⑨ 懸空引用＝佔位工單。
合一：⑩ 同一支 `search*` 跑耦合案例（全場「……」預算，director 是可分特例）。

## 已知簡化（下一步）

1. 河堤內容為代表節錄；擴到全 20 種／多場景是資料工作。
2. `*axes*` 手列；可由掃 `*ns*` 的 `(? key)` 自動導出。
3. 護欄目前字串式（非 canon）＋邏輯內嵌（變項×動詞在 `!` 裡）；可抽成節點級謂詞。
4. 真值域只示範排斥；完整 canon/蘊含/LOD 見 [uniform_probe/a3](../uniform_probe/a3/)。
5. `search*` 已是可分/耦合通用；**尚未把 gen_v0（Lua、act 粒度）實際改接到這支 CL `search*`**——跨語言，屬未來 port（gen_v0 的 `fill_sequence` 與本 `search*` 已證明同形）。
