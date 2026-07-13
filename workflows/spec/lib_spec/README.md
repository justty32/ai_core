# lib_spec.md

ai_core Python library 的 API 設計細節。

---

## 章節索引

- [`s1-io-entries.md`](./s1-io-entries.md)：§1 I/O 出入口（entries）——`able_in`/`able_out`、`mode`、`type`、`format`、`schema`、`channel_constraint`、`terminal_binding`，以及 Terminal 環境預定義 entry 名稱。
- [`register-intercept-lifecycle-state.md`](./register-intercept-lifecycle-state.md)：`register()` 與 `intercept()` 的宣告／攔截拆分設計；§2 生命週期（`lifecycle`）；§3 跨呼叫狀態（`state`、`state_dirs`）。
- [`s4-s5-resources-interruptible.md`](./s4-s5-resources-interruptible.md)：§5 可中斷性（`interruptible`）；§4 資源特性（`resources`：memory / gpu / time / disk / network）。
- [`s6-guarantee.md`](./s6-guarantee.md)：§6 執行保證（`guarantee`、`dry_run`），與 §5 可中斷性的交互關係表。
- [`s9-nondeterminism-composition-env.md`](./s9-nondeterminism-composition-env.md)：§9 確定性/隨機性（`nondeterministic`）；非軸欄位 `reliability`；未入軸決策 `memoized`；§7 組合模式（不定義 metadata）；§8 環境模式（不定義 metadata）。
