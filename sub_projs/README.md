# sub_projs — 次級物（子專案／封存）

← [INDEX](../INDEX.md)

本夾收各自成一攤的次級物：**半封存的舊實作版** `ver_1/`、**框架規劃地** `llm_forge/`、**動手實驗場** `galtxt/`、**獨立 C LLM 產物** `cllm/`、**舒適 CL 地基** `comfy/`、**OS-當-agent 試驗田** `handy/`、**歸一於路徑基材** `unipath/`。各目錄自帶入口，先讀入口再深入。

| 目錄 | 內容 | 入口 |
|------|------|------|
| `ver_1/` | 早期 Python 實作版的**封存**（src／tests／原型／範例）。已從主線退下，半封存狀態、不在現役維護鏈、不精整。 | （封存，不另設入口）|
| [llm_forge/](llm_forge/) | **框架規劃地（爐子）**：把 LLM 鍛成可靠管線的機制之家——固化階梯／雙錨驗證／評分級聯（套分層工作流模板）。目前僅規劃期文檔骨架，無程式碼。 | [AGENTS.md](llm_forge/AGENTS.md) |
| [galtxt/](galtxt/) | **動手實驗場（第一把刀）**：ai_core 北極星第一目標問題的落地執行處。**依賴 llm_forge**，先在此跑通想法、確定了才搬進 llm_forge 固化。套 workflows 分層模板；LLM 接口探索三線（try_1 s7／try_2 Lua／try_4 整合）已跑通洞見後**退出並移除工作樹**（細節留 git log）；現行推進線＝語料庫 `corpus/`。 | [AGENTS.md](galtxt/AGENTS.md) |
| [cllm/](cllm/) | **獨立 C LLM 產物**：把 LLM 收成一支對外 C ABI 共享庫 `libcllm.so`（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 `llm` unix filter CLI。純 C++、零腳本 VM、CMake+Ninja+vcpkg+glaze；離線 17/17 綠、Windows／Manjaro 雙機驗。源自 `galtxt/try_3`，收斂後抽離。 | [AGENTS.md](cllm/AGENTS.md) |
| [comfy/](comfy/) | **舒適 Common Lisp 地基（框架外實驗場）**：一層順手糖（`true`/`false`、`'a'` 字元）＋成熟可 VSCode/Alive debug 的 SBCL 環境。與 galtxt 的 s7 線並存、兩邊都推。零外部相依（只用 SBCL 內建 ASDF）。 | [README.md](comfy/README.md) |
| [handy/](handy/) | **路徑一試驗田（OS 當 AI agent）**：一組靈活小腳本／小程式集，拿現成程式（尤其 `cllm` 與其 tool）用腳本包裝、資料夾＝callable、按慣例組合。已有可用住戶——`llme`（cllm 多 endpoint dispatcher，換 endpoint 名＝換模型＋auto-inject api key，預設後端 DeepSeek）＋`zhtw`（繁中翻譯薄包裝，stdin 管線公民）；規劃中 `wf`（把任務派發給 claude code 這類 agent）、`daemon`（常駐 server）。資料夾採**分層工作流形式**（AGENTS→WORKFLOWS/INDEX→各住戶/工作流）。 | [AGENTS.md](handy/AGENTS.md) |
| [unipath/](unipath/) | **歸一於路徑基材（比 handy 更底層）**：「先歸一於路徑、後成局」願景的階段一落地——採 **Plan 9 思想＋9P 協議**、以 **FUSE 起步**，把一個活 process 的執行態環境暴露成可 walk 的路徑樹（`/proc` 式），外部 `ls`/`cat`/`echo` 即可定址讀寫。控制/資料抄 Plan 9 `ctl`/`data` 慣例。**Python 原型優先、暫不管效能；真 Plan 9 留作逃生門**。設計真源＝ai_core 筆記鏈（世界篇→兩層底座篇→實作路線篇）。現狀：原型待動工。 | [AGENTS.md](unipath/AGENTS.md) |
