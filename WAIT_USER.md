# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證 / 追認**才能繼續的事。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。**只列還沒做的**——做完即移除（歷史看 git log）。

## 待使用者項

- **確認後 push main 上累積的未推送 commit** — 含 `193ba68`（分層工作流模板）/ `132f500`（sub_projs 歸層）/ `8ea498a`（idea/spec/plan 歸類）及其後各 commit（llm_forge 骨架、本次結構重整），均已 commit 在 main、未 push（鐵律：push 先確認）。
- **回 POSIX 機補跑一次完整 `/test`** — 本輪重組零程式碼改動，Windows 機上 `smoke_test.py` 83 項斷言全過；但 pytest（需該機 `.venv`）與 `lib_smoke_test.py`（需 `AF_UNIX`）在 Windows 跑不了，回 POSIX 機順手驗一次即可安心。順帶實跑一次 router 的 echo 路由——本次重整把 `try_implement/tools/router.py` 裡早已失效的 `../funcs/echo.sh` 修正為 `../examples/echo.sh`（相對 routes.json 所在目錄解析），尚未實機驗證。
- **追認 DECISIONS.md D 區（agent 自行拍板的 7 項）** — 記憶化 cache key 組成／失效策略、Hub 定義、forge NDJSON 介面、Switch 條件表達、Layer 4 錯誤封套、交互 `max_rounds` 安全閥；均為 try_implement 原型的低風險拍板，待使用者追認或推翻。見 [try_implement/DECISIONS.md](try_implement/DECISIONS.md) D 區。
- **追認 DECISIONS.md「近期焦點①②」設計細節** — entry `format`/`schema` 值域與掛法、`reliability` 欄位形狀均為 Claude 拍板待追認。見 [try_implement/DECISIONS.md](try_implement/DECISIONS.md)「✅ 已收斂（2026-07-03）」。
- **決定 core_handy 是否升主線、Python 降參考；新 repo 是否 port 既有 daemon** — 2026-07-13 長談後的下一步待使用者主導。見 [SESSION-LOG.md](SESSION-LOG.md) 與 [ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
- **審議 crystallization_engine_blueprint.md（spec 候選）是否經 `/spec` 扶正** — 固化引擎 v0 工程藍圖已具函式簽章／編排流程／里程碑，標 `stage: spec-candidate`；待使用者裁決是否收斂進規範，關聯 [try_implement/DECISIONS.md](try_implement/DECISIONS.md) A4（組合軸推導）與 [docs/spec/composite_spec.md](docs/spec/composite_spec.md)。見 [ideas/crystallization_engine_blueprint.md](ideas/crystallization_engine_blueprint.md)。
