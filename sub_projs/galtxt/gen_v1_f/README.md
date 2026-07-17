# gen_v1_f —— 事實層地基・Fennel 移植版

← [galtxt INDEX](../INDEX.md)｜設計與規格出處：[gen_v1/](../gen_v1/README.md)（Lua 原版）＋[gen_v1/00_設計.md](../gen_v1/00_設計.md)

> **gen_v1 的 Fennel 重寫**（使用者指示，2026-07-17）：同一套事實庫（分層 LOD 網、
> 知識視圖、canon 鎖、兩道寫入門、六動詞），行為與九示範逐條對齊 Lua 版。
> 拆檔結構一比一對應（單檔 ≤120 行慣例）。
> 註：早前「Fennel 放棄」的定調由本實驗翻案重試——與 gen_v1（純 Lua）、gen_v1_l（Common Lisp）三線並排比較。

## 跑

```sh
cd gen_v1_f && fennel main.fnl     # Fennel 1.6+（CLI 自帶 .fnl require searcher）
```

## 檔案（與 gen_v1 一比一對應）

| 檔 | 對應 Lua 版 | 是什麼 |
|----|-------------|--------|
| [facts.fnl](facts.fnl) | facts.lua | 聚合入口：拼裝子模組＋`new` |
| [facts_store.fnl](facts_store.fnl) | facts_store.lua | 編譯期門①＋modify＋視圖＋dump |
| [facts_lod.fnl](facts_lod.fnl) | facts_lod.lua | 編譯期門②：refine 三形態 |
| [facts_query.fnl](facts_query.fnl) | facts_query.lua | 讀取五動詞 |
| [facts_tx.fnl](facts_tx.fnl) | facts_tx.lua | 執行期門：speculate 交易 |
| [facts_util.fnl](facts_util.fnl) | facts_util.lua | 共用小工具 |
| [seed.fnl](seed.fnl) | seed.lua | 河鹿堂素材灌庫 |
| [main.fnl](main.fnl) | main.lua | 跑批 → demos_graph（①～④）＋demos_gates（⑤～⑨） |

## Fennel 特有的取捨

- **迴圈早退**：Lua 的 `return`/`break` 中途退出，Fennel 版改用 `var` 旗標＋守衛（`(when (= err nil) …)`）——首個命中者贏，確定性語義不變。
- **巢狀早退的 check**：`tx.check` 的多層 return 拆成 `check-act` 輔助函式（回 nil／錯誤訊息）。
- **nil 安全下行**：`(?. act :effects :reveal)` 取代 Lua 的 `(act.effects or {}).reveal or {}`。
- 多值回傳（`(values nil "不可見")`）、metatable、`pcall` 皆與 Lua 同構——畢竟編譯目標就是 Lua。
