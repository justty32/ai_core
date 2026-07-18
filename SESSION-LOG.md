# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——里程碑史交給 git log。待**使用者**親自驗證／追認的另見 [WAIT_USER.md](WAIT_USER.md)。

> 條目格式：每條只留**一行 open 狀態 + 指向細節的連結**，完成即整條刪除。

## 最新進度（open）

- **最硬未決題：固化引擎手動 vs 自動** — 標「這題優先」，見 [roadmap.md](workflows/roadmap/README.md) §3.6 / §8。
- **v0 最小垂直切片尚未動工** — 一個真實框架 → 一次聰明模型生資產 → 笨模型 + 行數助手 + retry/guard，見 [roadmap.md](workflows/roadmap/README.md) §6。
- **第一目標問題構想上已換成 galgame 台詞生成（落地為 galtxt，依賴 llm_forge 框架），但 roadmap.md §5 尚未動手術** — 北極星不動、入門切口換材料不換架構；正式改稿暫緩到討論收斂。詳見最新長談 [workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
- **刻意暫緩至 v0：A4（組合軸推導）＋ B 系列（B1 語意欄位 / B2 共用模組 / B3 沙箱）** — 留給 v0 切片逼出優先序，見 [roadmap.md](workflows/roadmap/README.md) §7。
- **workflows/spec/overview.md 仍是「待填」stub** — 規範家族缺總覽定位（這套設計模式是工具集？設計模式？執行組織框架？），見 [workflows/spec/overview.md](workflows/spec/overview.md)「待填：本質是什麼」節。
- **galtxt/llm_forge 角色切分（新定調 7/13，文檔已對齊）**：`sub_projs/galtxt/`＝**動手實驗場**（先執行、跑通想法、galgame 生成器本身），`sub_projs/llm_forge/`＝**框架/爐子**（確定的機制才從 galtxt 搬來固化）；galtxt 依賴 llm_forge。兩邊 AGENTS/INDEX/WORKFLOWS 均已改述對齊。galtxt 已長出**兩個玩具實驗場**（刻意不套框架、先跑通 LLM 接口）：`try_1`＝**s7 Scheme** 版、`try_2`＝**C++ 內嵌 Lua 5.5** 版，兩條並跑互為對照，離線 E2E 皆綠、schema 生成 CLI 皆落地；真後端實跑待使用者。詳見 [sub_projs/galtxt/SESSION-LOG.md](sub_projs/galtxt/SESSION-LOG.md)。
- **舒適 CL 地基 `comfy/` 已立且環境全通（框架外實驗場，與 galtxt s7 線並存、兩邊都推）**：SBCL 2.6.6＋Alive（LSP／REPL／inline-eval／高亮）**使用者已在 VSCode 實測成功**；`:comfy` 糖層（`true`/`false`＋`'a'` 字元＋C 風格字串轉義 `"a\nb"`）測試全綠；Quicklisp＋`com.inuoe.jzon` JSON round-trip 過；高亮＝`editor/vscode-comfy/` 注入文法。**跨平台**：Windows／Manjaro 共用 repo，`.vscode/settings.json` 改本機檔＋兩平台模板，Manjaro 首次設定清單見 README。相依分層：糖本體零外部相依，庫走 Quicklisp。下一步：更多順手糖／把 s7「schema→CLI 由同像性生成」洞見在 CL 用 macro 重生。細節見 [sub_projs/comfy/README.md](sub_projs/comfy/README.md)。

- **路徑一開工：`handy` 子專案（把 OS 當 AI agent 的落地試驗田）**（2026-07-18）：一組靈活小腳本／小程式集，拿現成程式（尤其 `cllm` 與其 tool）用腳本包裝、資料夾＝callable、按慣例組合。**已有兩個可用住戶**：`llme`（cllm 多 endpoint dispatcher，Fennel 單檔，`llme <endpoint>` → `llm --config configs/<endpoint>.json`，真後端 LM Studio 驗過、串流＋多 endpoint 皆綠）＋`zhtw`（薄包裝範例：固定取樣參數＋system＋前置轉呼 llme）。**技術棧定調**：腳本層 LuaJIT＋Fennel（少 Python）；daemon（`--serve`）暫緩、觸發＝跨呼叫共享狀態非省啟動。**Lua/Fennel 那條「舒適地基候選」即在此落地**（comfy SBCL 線保留並存）。設計脈絡見 [notes/20260718-0924-路徑一-daemon與llme兩塊試驗田.md](workflows/notes/20260718-0924-路徑一-daemon與llme兩塊試驗田.md)，入口 [sub_projs/handy/AGENTS.md](sub_projs/handy/AGENTS.md)。**open 尾巴**：daemon 未動；真網路後端 endpoint 待使用者填真 key；`llm` 加 `--system` 待辦（見 cllm SESSION-LOG）。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

## 不屬任何工作流的進度

- （無）
