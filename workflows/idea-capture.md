# idea-capture — 頭腦風暴軌（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

從 TTemp 借來的**頭腦風暴軌**，與本 repo 的規劃／規範工作並行、互不干擾。兩種腦暴動作，產物落在 [ideas/brainstorm/](ideas/brainstorm/)。

> 口述線一條龍（原文逐字 → 初步整理 → 匯總筆記）已獨立成自己的工作流 [intake](intake/README.md)。

## 兩種動作

| 動作 | 用途 | 產物落點 |
|------|------|---------|
| **critique** | 頭腦風暴・找漏洞（指出哪裡有錯／考慮不周）| `ideas/brainstorm/` |
| **expand** | 頭腦風暴・擴展靈感（把點子變大、變多）| `ideas/brainstorm/` |

## 備註

原本這兩個動作的 LLM 加工是 shell out 給 ai_core 自己的 `idea.py`（dogfood 自家 LLM 基礎設施，經元件 1 entry manager 路由、宣告第九軸 `nondeterministic:true`）。該工具已隨 Python 實作封存進 [ver_1](../sub_projs/ver_1/try_implement/README.md)——當前無在跑的實作，要用把它當參考。
