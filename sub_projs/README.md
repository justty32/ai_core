# sub_projs — 次級物（子專案／封存）

← [INDEX](../INDEX.md)

本夾收各自成一攤的次級物：**半封存的舊實作版** `ver_1/`、**框架規劃地** `llm_forge/`、**動手實驗場** `galtxt/`、**獨立 C LLM 產物** `cllm/`、**舒適 CL 地基** `comfy/`、**OS-當-agent 試驗田** `handy/`、**歸一於路徑基材** `unipath/`、**handy 多語言移植** `cllm-apps/`、**cllm 綁定遊樂場** `cllm-lab/`、**Common Lisp 遊樂場** `cl-lab/`、**Janet 遊樂場** `janet-lab/`。各目錄自帶入口，先讀入口再深入。

| 目錄 | 內容 | 入口 |
|------|------|------|
| `ver_1/` | 早期 Python 實作版的**封存**（src／tests／原型／範例）。已從主線退下，半封存狀態、不在現役維護鏈、不精整。 | （封存，不另設入口）|
| [llm_forge/](llm_forge/) | **框架規劃地（爐子）**：把 LLM 鍛成可靠管線的機制之家——固化階梯／雙錨驗證／評分級聯（套分層工作流模板）。目前僅規劃期文檔骨架，無程式碼。 | [AGENTS.md](llm_forge/AGENTS.md) |
| [galtxt/](galtxt/) | **動手實驗場（第一把刀）**：ai_core 北極星第一目標問題的落地執行處。**依賴 llm_forge**，先在此跑通想法、確定了才搬進 llm_forge 固化。套 workflows 分層模板；LLM 接口探索三線（try_1 s7／try_2 Lua／try_4 整合）已跑通洞見後**退出並移除工作樹**（細節留 git log）；現行推進線＝語料庫 `corpus/`。 | [AGENTS.md](galtxt/AGENTS.md) |
| [cllm/](cllm/) | **獨立 C LLM 產物**：把 LLM 收成一支對外 C ABI 共享庫 `libcllm.so`（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 `llm` unix filter CLI。純 C++、零腳本 VM、CMake+Ninja+vcpkg+glaze；離線 17/17 綠、Windows／Manjaro 雙機驗。源自 `galtxt/try_3`，收斂後抽離。 | [AGENTS.md](cllm/AGENTS.md) |
| [comfy/](comfy/) | **舒適 Common Lisp 地基（框架外實驗場）**：一層順手糖（`true`/`false`、`'a'` 字元）＋成熟可 VSCode/Alive debug 的 SBCL 環境。與 galtxt 的 s7 線並存、兩邊都推。零外部相依（只用 SBCL 內建 ASDF）。 | [README.md](comfy/README.md) |
| [handy/](handy/) | **路徑一試驗田（OS 當 AI agent）**：一組靈活小腳本／小程式集，拿現成程式（尤其 `cllm` 與其 tool）用腳本包裝、資料夾＝callable、按慣例組合。已有可用住戶——`llme`（cllm 多 endpoint dispatcher，換 endpoint 名＝換模型＋auto-inject api key，預設後端 DeepSeek）＋`zhtw`（繁中翻譯薄包裝，stdin 管線公民）；規劃中 `wf`（把任務派發給 claude code 這類 agent）、`daemon`（常駐 server）。資料夾採**分層工作流形式**（AGENTS→WORKFLOWS/INDEX→各住戶/工作流）。 | [AGENTS.md](handy/AGENTS.md) |
| [unipath/](unipath/) | **歸一於路徑基材（比 handy 更底層）**：「先歸一於路徑、後成局」願景的階段一落地——採 **Plan 9 思想＋9P 協議**、以 **FUSE 起步**，把活 process 的執行態環境暴露成可 walk 的路徑樹（`/proc` 式），`ls`/`cat`/`echo` 即可定址讀寫。控制/資料抄 Plan 9 `ctl`/`data`/`status` 慣例。**step 1–6 全綠**（假樹→執行態→跨 process→真 9P2000→tick 回合制→腳本化 NPC＋巢狀 tick，規則語言＝Janet）＋C++/Fennel 版跨語言 9P 互通（語言中立實證），核心 v9fs 真掛載驗過。已套分層工作流正規化、程式碼每檔 <100 行。附 8 篇作業系統背景 docs。設計真源＝ai_core 筆記鏈。 | [AGENTS.md](unipath/AGENTS.md) |
| [cllm-apps/](cllm-apps/) | **handy 四工具多語言移植（原 `~/code/cllm-apps`，2026-07-19 納入）**：把 [handy](handy/AGENTS.md) 的 `llme`/`zhtw`/`wf`/`mail` 以 **shell-out 方式**移到各語言（真正的「手」仍轉呼 `llm`/`claude`/同語言 sibling，不用 cllm binding），行為 1:1 對齊 Fennel 原型。完成品 `python-handy/`／`cpp-handy/`／`lisp-handy/`／`janet-handy/`（四工具離線 dry-run 全綠）；早期探索 `*-try-1/`（cpp/janet/lisp/lua）。每 app 附 `configs/`（deepseek keyless＋anthropic-proxy）＋`scripts/{vendor.sh,up.sh}` 串接 cllm 周邊（anthropic-proxy／llm-login）。契約權威＝`HANDY-PORT-SPEC.md`、總覽＋視覺速覽＝`README.md`／`overview.html`。`build/`／`vendor/`／`.run/` 自帶 `.gitignore` 不進版控、可 regen。 | [README.md](cllm-apps/README.md) |
| [cllm-lab/](cllm-lab/) | **cllm 綁定十語言遊樂場（原 `~/code/labs/cllm-lab`，2026-07-19 納入）**：用任何語言呼叫 LLM（`libcllm.so` 的 C ABI，唯一入口 `llm_ask`）。每語言一夾、`play.*` 是完整範例起手檔（ask／串流／schema／tools／media 全示範），改它即開發。真相源＝[cllm](cllm/AGENTS.md) 的 `bindings/<lang>/example.*`；lab 是遊樂場。上手教學見 `GUIDE.md`、一頁速查 `cheatsheet.html`。 | [README.md](cllm-lab/README.md) |
| [cl-lab/](cl-lab/) | **Common Lisp 開發遊樂場（原 `~/code/labs/cl-lab`，2026-07-19 納入）**：能跑的 SBCL/ASDF 專案骨架＋中文速查（`cl-cheatsheet.html`）＋整套分層工作流（AGENTS/INDEX/DEV-GUIDE/SESSION-LOG）。支撐 [cllm-apps](cllm-apps/HANDY-PORT-SPEC.md) 的 lisp-handy 與 [comfy](comfy/README.md) 的 CL 線。獨立執行檔 `build/cl-lab`（45M SBCL image）走 `scripts/run.sh build` 重生、不進版控。 | [README.md](cl-lab/README.md) |
| [janet-lab/](janet-lab/) | **Janet 開發遊樂場（原 `~/code/labs/janet-lab`，2026-07-19 納入）**：能跑的 jpm 專案（Janet 1.41.2＋spork＋janet-lsp，裝在 ~/.local）＋12 篇中文教學（`docs/` 含 fiber/macro/C 互通/pipeline-signal）＋單頁 cheatsheet。**是 [unipath](unipath/AGENTS.md) 換 Janet 規則語言的學習底本**。 | [README.md](janet-lab/README.md) |
