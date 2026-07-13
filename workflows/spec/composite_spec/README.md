# composite_spec.md

`lib_spec.md` 定義各軸的原子性 metadata fields——每個軸描述程式的一個獨立特性。本檔定義**複合式規範（composite conventions）**：橫跨多軸的標準模式，library 為這些模式提供一體化的配套設施。

複合式規範不新增孤立的概念，而是說明「哪些軸的特定組合，在實作層面形成一個具名慣例」，以及這個慣例帶來的附加 metadata fields 與 library 工具。

---

## 複合規範列表

| 慣例 | 觸發條件 | 涵蓋軸 |
|---|---|---|
| 標準狀態目錄慣例 | `state: "stateful_external"` + terminal 環境 | §1、§3、§5、§6 |
| 中斷恢復慣例 | `state: "stateful_external"` + (`interruptible ∈ {resumable, rollback, resettable}` 或 `guarantee: "transactional"`) + terminal | §3、§5、§6 |

---

## 章節索引

- [state-dir-convention.md](state-dir-convention.md) — 標準狀態目錄慣例：`state: "stateful_external"` 程式在 terminal 環境下，`.config`／`.cache`／`.state`／`.data` 四個標準目錄的結構、語意與跨軸涵蓋。
- [interruption-recovery.md](interruption-recovery.md) — 中斷恢復慣例：建立在標準狀態目錄慣例之上，規定 `.state` 內恢復記錄（`recovery.json`）的格式、恢復模式（resume／rollback／reset）與啟動時的標準恢復演算法。
