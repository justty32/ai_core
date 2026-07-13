# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證 / 追認**才能繼續的事。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。**只列還沒做的**——做完即移除（歷史看 git log）。

## 待使用者項

- **確認後 push main 上累積的未推送 commit** — 含 `193ba68`（分層工作流模板）/ `132f500`（sub_projs 歸層）/ `8ea498a`（idea/spec/plan 歸類）及其後各 commit（llm_forge 骨架、本次結構重整），均已 commit 在 main、未 push（鐵律：push 先確認）。
- **審議 crystallization_engine_blueprint.md（spec 候選）是否經 spec 工作流扶正** — 固化引擎 v0 工程藍圖已具函式簽章／編排流程／里程碑，標 `stage: spec-candidate`；待使用者裁決是否收斂進規範，關聯 [workflows/spec/composite_spec.md](workflows/spec/composite_spec.md)。見 [workflows/ideas/crystallization_engine_blueprint.md](workflows/ideas/crystallization_engine_blueprint.md)。
