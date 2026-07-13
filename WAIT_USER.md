# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證 / 追認**才能繼續的事。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。**只列還沒做的**——做完即移除（歷史看 git log）。

## 待使用者項

- **追認 DECISIONS.md D 區（agent 自行拍板的 7 項）** — 記憶化 cache key 組成／失效策略、Hub 定義、forge NDJSON 介面、Switch 條件表達、Layer 4 錯誤封套、交互 `max_rounds` 安全閥；均為 try_implement 原型的低風險拍板，待使用者追認或推翻。見 [try_implement/DECISIONS.md](try_implement/DECISIONS.md) D 區。
- **追認 DECISIONS.md「近期焦點①②」設計細節** — entry `format`/`schema` 值域與掛法、`reliability` 欄位形狀均為 Claude 拍板待追認。見 [try_implement/DECISIONS.md](try_implement/DECISIONS.md)「✅ 已收斂（2026-07-03）」。
- **決定 core_handy 是否升主線、Python 降參考；新 repo 是否 port 既有 daemon** — 2026-07-13 長談後的下一步待使用者主導。見 [SESSION-LOG.md](SESSION-LOG.md) 與 [ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](ideas/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
