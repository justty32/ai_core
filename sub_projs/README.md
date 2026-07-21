# sub_projs — 次級物（子專案／封存）

← [INDEX](../INDEX.md)

本夾收各自成一攤的次級物。2026-07-20 結構重整：**cllm 相關的收進 `cllm/` 傘**（產物＋周邊），**各語言 lab 收進 `lang-labs/` 傘**；其餘維持頂層。各目錄自帶入口，先讀入口再深入。

## 頂層佈局

| 目錄 | 內容 | 入口 |
|------|------|------|
| [cllm/](cllm/) | **cllm 傘**：獨立 C LLM 產物（`core/`）＋其周邊。見下「cllm 傘」。 | [README.md](cllm/README.md) |
| [lang-labs/](lang-labs/) | **各語言開發遊樂場傘**：Common Lisp（`cl-lab/`，內含 `comfy/`）＋Janet（`janet-lab/`）。見下「lang-labs 傘」。 | [README.md](lang-labs/README.md) |
| [galtxt/](galtxt/) | **動手實驗場（第一把刀）**：ai_core 北極星第一目標問題的落地執行處（galgame 台詞生成）。套 workflows 分層模板；現行推進線＝語料庫 `corpus/`＋gen 生成器＋uni 統一 kernel。〔2026-07-20 開發暫緩〕 | [AGENTS.md](galtxt/AGENTS.md) |
| [handy/](handy/) | **路徑一試驗田（OS 當 AI agent）**：一組靈活小腳本／小程式集，拿現成件用薄殼包裝、資料夾＝callable、按慣例組合。**2026-07-21 重構完成**：LLM 地基＝`util/llm`（litellm 薄包裝，形狀鏡像已封存的 `pllm`）＋共用 lib `util`（`config`／`jsref`）＋住戶① `llme`。舊 Fennel 版住戶（`llme`/`zhtw`/`wf`/`mail`、`hermy`）與 `pllm` 全凍結於 `archive/`。⚠ 刻意破例依賴 litellm 的**試裝階段**。 | [README.md](handy/README.md) |
| [unipath/](unipath/) | **歸一於路徑基材（比 handy 更底層）**：Plan 9 思想＋9P 協議、FUSE 起步，把活 process 執行態暴露成可 walk 路徑樹。step 1–6 全綠、規則語言＝Janet。**Windows 跑不了**（FUSE，僅 Linux／macOS）。 | [AGENTS.md](unipath/AGENTS.md) |
| [llm_forge/](llm_forge/) | **框架規劃地（爐子）**：把 LLM 鍛成可靠管線的機制之家——固化階梯／雙錨驗證／評分級聯。目前僅規劃期文檔骨架、無程式碼；galtxt 跑通的機制才搬來固化。 | [AGENTS.md](llm_forge/AGENTS.md) |
| `ver_1/` | 早期 Python 實作版的**封存**（src／tests／原型／範例）。已從主線退下，半封存狀態、不在現役維護鏈、不精整。 | （封存，不另設入口）|

## cllm 傘（[cllm/](cllm/)）

| 目錄 | 內容 | 入口 |
|------|------|------|
| [cllm/core/](cllm/core/) | **獨立 C LLM 產物**：把 LLM 收成一支對外 C ABI 共享庫 `libcllm.so`／`.dll`（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 `llm` unix filter CLI。純 C++、CMake+Ninja+vcpkg+glaze；離線 35/35 綠、Windows（mingw64 g++16.1）／Manjaro 雙機驗。源自 `galtxt/try_3`，收斂後抽離。 | [AGENTS.md](cllm/core/AGENTS.md) |
| [cllm/apps/](cllm/apps/) | **handy 四工具多語言移植**（原 cllm-apps）：把 handy 的 `llme`/`zhtw`/`wf`/`mail` 以 **shell-out 方式**移到各語言（轉呼 `llm`/`claude`/sibling，不用 binding），行為 1:1 對齊 Fennel 原型。完成品 `python-handy`／`cpp-handy`／`lisp-handy`／`janet-handy`。契約權威＝`HANDY-PORT-SPEC.md`。 | [README.md](cllm/apps/README.md) |
| [cllm/lab/](cllm/lab/) | **cllm 綁定十語言遊樂場**（原 cllm-lab）：用任何語言呼叫 LLM（`libcllm` 的 C ABI）。每語言一夾、`play.*` 是完整範例起手檔。真相源＝core 的 `bindings/<lang>/example.*`。上手教學 `GUIDE.md`、速查 `cheatsheet.html`。 | [README.md](cllm/lab/README.md) |

## lang-labs 傘（[lang-labs/](lang-labs/)）

| 目錄 | 內容 | 入口 |
|------|------|------|
| [lang-labs/cl-lab/](lang-labs/cl-lab/) | **Common Lisp 開發遊樂場**：能跑的 SBCL/ASDF 骨架＋中文速查＋整套分層工作流。支撐 [cllm/apps](cllm/apps/HANDY-PORT-SPEC.md) 的 lisp-handy。**內含 [comfy/](lang-labs/cl-lab/comfy/README.md)**（舒適 CL 地基：順手糖＋VSCode/Alive debug 環境，框架外實驗場）。 | [README.md](lang-labs/cl-lab/README.md) |
| [lang-labs/janet-lab/](lang-labs/janet-lab/) | **Janet 開發遊樂場**：能跑的 jpm 專案（Janet 1.41.2＋spork＋janet-lsp）＋中文教學＋cheatsheet。**是 [unipath](unipath/AGENTS.md) 換 Janet 規則語言的學習底本**。 | [README.md](lang-labs/janet-lab/README.md) |
