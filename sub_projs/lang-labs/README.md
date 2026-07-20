# lang-labs/ — 各語言開發遊樂場傘

← [sub_projs](../README.md)

各語言的「能跑的專案骨架 ＋ 中文教學／速查」遊樂場全收這把傘底下（2026-07-20 結構重整）。這些是**支撐物**：給別的子專案（cllm 綁定、handy 移植、unipath 規則語言）當學習底本與環境地基，本身不是主線產物。

| 目錄 | 是什麼 | 入口 |
|------|--------|------|
| [cl-lab/](cl-lab/) | **Common Lisp（SBCL）遊樂場**：能跑的 ASDF 專案骨架＋中文速查＋整套分層工作流。支撐 [cllm/apps](../cllm/apps/README.md) 的 lisp-handy。**內含 [comfy/](cl-lab/comfy/README.md)**（舒適 CL 地基：順手糖＋VSCode/Alive debug 環境）。 | [README.md](cl-lab/README.md) |
| [janet-lab/](janet-lab/) | **Janet 遊樂場**：能跑的 jpm 專案（Janet＋spork＋janet-lsp）＋中文教學＋cheatsheet。**是 [unipath](../unipath/AGENTS.md) 換 Janet 規則語言的學習底本**。 | [README.md](janet-lab/README.md) |
