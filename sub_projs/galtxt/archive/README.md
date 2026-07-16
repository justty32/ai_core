# archive — 已封存的玩具實驗場

← [galtxt INDEX](../INDEX.md)

galtxt 早期「LLM 接口探索」的三條玩具實驗場，**已於 2026-07-16 封存**——退出現役維護鏈、不再精整。三條線各自已把該證明的洞見跑通（見下），階段性任務完成；封存保留脈絡與 durable 細節，日後要參考／port 隨時回來翻。**各 `try_*/README.md` ＋ [common/gotchas](../workflows/common/gotchas.md) ＋ git log 是細節真相源**，本檔只做導覽。

> 純 C++ 那條（原 `try_3`）不在此——它已收斂成兩交付物、**畢業抽離成獨立 sub_proj [cllm](../../cllm/README.md)**（非封存）。

| 目錄 | 內容 | 證明的洞見 |
|------|------|-----------|
| [`try_1/`](try_1/README.md) | **s7 Scheme** 版 LLM 接口：`llm.scm`＋獨立 `json.scm`，`*llm-schema*` 唯一真相源用 `eval` 生成薄殼 `define*`（schema 表生成簽章），argv-aware host `s7host.c`＋`shim_include/`。真後端（LM Studio）已打通、Windows／Manjaro 雙機驗。 | schema 生成函式簽章＋argv host＋Lisp 同像性 |
| [`try_2/`](try_2/README.md) | **C++ 內嵌 Lua 5.5** 版：`host.cpp`＋native C `cjson.c`／`http.c`（零外部 curl、WinHTTP／libcurl），schema 唯一真相源＋由 schema 生成 `--flag` CLI，Lua 格式輸出（table 進出）＋UTF-8 串流。真後端已打通、雙機驗。 | schema 驅動 CLI／JSON／串流＋Lua table 進出 |
| [`try_4/`](try_4/README.md) | **三線整合**：CMake 借編 try_3 核心 `.cpp`（不分叉），C++ 扛重活（傳輸／glaze 反射／schema／`ask` 及三擴充），s7＋Lua 當薄層吃同一 function API。`ask` 完整表面＋工具／結構化／多媒體三擴充綁上兩 VM、真後端整條驗過（5 能力 × 2 VM）。 | 一套 C++ 核心、兩層腳本薄殼吃同一 API |

原三線的 open 待辦（`*llm-schema*` 生 `--flag` CLI 薄殼、三擴充接串流、腳本側表描述生 schema…）隨封存一併擱置，不再列為活狀態（見 [SESSION-LOG](../SESSION-LOG.md)）。
