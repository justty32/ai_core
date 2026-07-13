# doc_01_nature_overview

- **來源**：`core_nature/overview.md`
- **狀態**：定案規範（部分待填）
- **一行摘要**：這套設計模式的本質定位探究——從 terminal 出發釐清函式形式內核，目標平台 POSIX（Windows 不考慮），「本質是什麼」仍標待填。

> 交叉引用：九軸詳規見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)、library 契約見 [doc_06_lib_contract.md](doc_06_lib_contract.md)、執行模型見 [doc_02_terminal_model.md](doc_02_terminal_model.md)、執行形式見 [doc_03_execution_forms.md](doc_03_execution_forms.md)。

---

這套設計模式的本質探究。從 terminal 環境出發，理清函式形式的內核，再擴展至其他環境。

---

## 為什麼從 terminal 出發

函式的執行形式（one-shot、multi-shot、persistent 等）是跨環境的普遍概念，無論程式內函數、terminal、分散式系統，內核不變，只是外在表現與限制隨環境而異。

Terminal（POSIX）是最純粹的起點：限制明確、介面統一、無框架干擾。先在此把模式的內核想清楚，再往外擴展。

**目標平台：POSIX 系統（含 pipeline）。Windows 不在考慮範圍。**

---

## 待填：本質是什麼

（這套設計模式的精確定位——工具集？設計模式？執行組織框架？待討論後補充）
