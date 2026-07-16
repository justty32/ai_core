# sub_projs — 次級物（子專案／封存）

← [INDEX](../INDEX.md)

本夾收各自成一攤的次級物：**半封存的舊實作版** `ver_1/`、**框架規劃地** `llm_forge/`、**動手實驗場** `galtxt/`、**獨立 C LLM 產物** `cllm/`、**舒適 CL 地基** `comfy/`。各目錄自帶入口，先讀入口再深入。

| 目錄 | 內容 | 入口 |
|------|------|------|
| `ver_1/` | 早期 Python 實作版的**封存**（src／tests／原型／範例）。已從主線退下，半封存狀態、不在現役維護鏈、不精整。 | （封存，不另設入口）|
| [llm_forge/](llm_forge/) | **框架規劃地（爐子）**：把 LLM 鍛成可靠管線的機制之家——固化階梯／雙錨驗證／評分級聯（套分層工作流模板）。目前僅規劃期文檔骨架，無程式碼。 | [AGENTS.md](llm_forge/AGENTS.md) |
| [galtxt/](galtxt/) | **動手實驗場（第一把刀）**：ai_core 北極星第一目標問題的落地執行處。**依賴 llm_forge**，先在此跑通想法、確定了才搬進 llm_forge 固化。套 workflows 分層模板；LLM 接口探索三線（try_1 s7／try_2 Lua／try_4 整合）已跑通洞見後**退出並移除工作樹**（細節留 git log）；現行推進線＝語料庫 `corpus/`。 | [AGENTS.md](galtxt/AGENTS.md) |
| [cllm/](cllm/) | **獨立 C LLM 產物**：把 LLM 收成一支對外 C ABI 共享庫 `libcllm.so`（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 `llm` unix filter CLI。純 C++、零腳本 VM、CMake+Ninja+vcpkg+glaze；離線 17/17 綠、Windows／Manjaro 雙機驗。源自 `galtxt/try_3`，收斂後抽離。 | [AGENTS.md](cllm/AGENTS.md) |
| [comfy/](comfy/) | **舒適 Common Lisp 地基（框架外實驗場）**：一層順手糖（`true`/`false`、`'a'` 字元）＋成熟可 VSCode/Alive debug 的 SBCL 環境。與 galtxt 的 s7 線並存、兩邊都推。零外部相依（只用 SBCL 內建 ASDF）。 | [README.md](comfy/README.md) |
