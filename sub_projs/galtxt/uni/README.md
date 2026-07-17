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
| [search.lisp](search.lisp) | 36 | **共用 search kernel**：搜 `*env*`（風格軸）→護欄剪枝→成本(prior＋intent＋history)取最小；`direct`／`direct-n` |
| [main.lisp](main.lisp) | 51 | load 定序＋九 demo |

## 兩面如何合一

- **表示**（kernel＋scene）：一張 `*ns*` 表；節點是 `(? key …)`（env 選）＋`(! …)`（邏輯）＋子引用折成的 realize。粒度由 refs 湧現、不限四種；`真值?` 分事實/字面兩域；懸空引用＝佔位。
- **演算法**（search）：`*env*`（母題/情緒/選路）**就是搜尋維度**——`(? key …)` 每個選擇點都是待搜的 env 值。`direct`＝枚舉 `*axes*` 笛卡爾積、護欄剪枝（非 canon 不可行）、`prior＋icost(intent)＋history` 取最小。
- **接縫一句話**：**一棵統一節點樹 ＋ 一個搜 `*env*` 的 search kernel。** gen_v0＝搜 act 粒度的 env、本 director＝搜句式/填充粒度的 env——同一台機器。因 env 鍵跨節點共享（耦合），這裡走全域枚舉；若鍵各節點獨立（可分），退化成 realizer_l 的逐槽搜——[一台搜尋機](../一台搜尋機.md) 說的「同一台機器兩種耦合」在此成真。

## 九個 demo

表示：① 一種節點跑整場｜② 粒度湧現 0/1/2/3｜③ 真值域 vs 字面域同表分域。
演算法：④ 搜最優（無 intent）｜⑤ intent 甜 vs 靜搜出不同風格｜⑥ 護欄剪枝（含『咖啡廳』的場成本=nil 被排除）｜⑦ `direct-n` 自動生 3 種台詞流。
結構：⑧ 分支＝普通節點（選路由 env）｜⑨ 懸空引用＝佔位工單。

## 已知簡化（下一步）

1. 河堤內容為代表節錄；擴到全 20 種／多場景是資料工作。
2. `*axes*` 手列；可由掃 `*ns*` 的 `(? key)` 自動導出。
3. 護欄目前字串式（非 canon）＋邏輯內嵌（變項×動詞在 `!` 裡）；可抽成節點級謂詞。
4. 真值域只示範排斥；完整 canon/蘊含/LOD 見 [uniform_probe/a3](../uniform_probe/a3/)。
5. 尚未與 gen_v0（act 粒度）真正共用同一支 `search` 函式——那是「一台搜尋機」待辦的最後一里。
