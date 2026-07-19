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

- **呼叫閘道加 `.so` kind ＋ 值跨邊界（設計已收斂、未動工）**（2026-07-18）：script 與 `.so` 對等共存，統合在「呼叫綁定」層——給既有 `lib/call.py`（in_process/subprocess/http）加第四 kind `dlopen`。核心決定：**一個 typed `Value`、兩種物理形態**（in-process 傳 C struct 零序列化／跨行程序列化成 JSON，CBOR 延後），故 typed 不逆 `spec/data_format.md`（它只管跨行程 wire）；`.so→腳本` 兩條都要，門檻＝**可信∧支援→in-process，其餘 exec 地板**。`.so` 端 ABI 沿用 cllm `llm_ask` 範式。**續談加了**：①「邊界 × 知不知道簽名」兩根正交軸（JSON 是 unknown 稅非 out 稅）＋萬能入口符號 `call(fn_name,Value*,Value*)`；② LLM 砍平人因軸→語言選擇退化成純能力題（給「語言中立」補硬理由）；③〔決定〕**收回 drop-shebang**：每檔兩面孔＝CLI 走 shebang、`.so` 走 `--gen-h`（吐 `.h`＋`.c` marshaling shim，腳本的 known/typed in-process 路徑，泛型 adapter 當預設、生成 shim opt-in）；④〔提案未追認〕第三物理形態＝shm 無指標 binary（把 C ABI 偷塞進 out-process，四格連續譜）。**open 尾巴**：加 `kind:"so"`（先 exec 等價）、定 `Value`＋LuaJIT FFI mirror、`--gen-h`/`--emit-header`、in-process 資源上限沿用 note_06 B3 未解洞。設計脈絡見 [notes/20260718-2324-so與腳本共存-呼叫閘道-value跨邊界.md](workflows/notes/20260718-2324-so與腳本共存-呼叫閘道-value跨邊界.md)。**已過一致性審查、訂正 4 處**（錨定事實逐條核對，7 條無造假）：①行數 129→128；②cllm「31/31 綠」錨錯檔＋repo 自身另有 35/35 待對齊；③`compose_meta.py`→`_core.py`；④萬能入口撞名拆為 `call_value`（in-process `Value*`）／`call_bytes`（序列化）。

- **「世界（OS-as-game）」願景篇成形（純思考層、未動工）**（2026-07-19）：把整台 OS 當**回合制世界**——檔案系統即場景、資料夾即空間/時間/複雜度三重分層、process/腳本即 NPC、使用者即玩家兼角色、**tick** 即世界時鐘（等所有 NPC＋玩家、玩家喊「這輪到此」即終止、收尾暫存進度＋選擇性 git commit＋server 假存活）。**核心收斂**：①**兩種世界**＝tick 驅動資料夾（慢·明朗·純 OS）vs 驅動檔案→process（快·隱含，對外 NPC/對內隱性 NPC）＝分形內縮，**這解掉了與 0718 in-process/.so 的路線衝突**（那套活在快世界）；②**兩世界通聯協議**雛形＝0718 的 `Value` 兩形態；③**Lisp REPL 當統一界面**（shell→REPL、改檔→改 process 內 list，同像性讓兩者同為可操作物）；④做什麼＝**Lisp lib ＋ OS 層級標準**兩者互補；⑤再一根**動態/靜態軸**（記憶體/硬碟，process/thread 也成檔案）＋靈活分層＋層間協議＝複雜度分級；⑥**通用協議＝檔案（Unix 檔案哲學）**，檔案本質＝路徑(where)＋副檔名(what)，**三軸全收斂到路徑**（空間＝路徑、時間＝git、複雜度＝資料夾深度）。**open 尾巴**：LOD 略圖規則／NPC 粒度／通聯協議細節／兩套庫＋OS 標準／process-as-file／相對論式時間／住民 vs handy「住戶」命名對齊——皆未動工。〔界線〕「不凋花／無盡覺醒＋LLM 鷹架→採集」是**另一套、脫鉤**。全文見 [notes/20260719-1150-世界-os-as-game-tick-npc-檔案系統即場景.md](workflows/notes/20260719-1150-世界-os-as-game-tick-npc-檔案系統即場景.md)。

- **世界篇後續：兩層底座重解＋時間軸函數化（純思考層、未動工）**（2026-07-19）：接世界篇。**核心修訂**：兩層底座從「並列」改為**有序兩階段**——①階段一 Linux-as-Lisp 的**真正本質＝歸一於路徑**（unify-to-path：靜態物→可執行/執行中的東西→呼叫與使用方式，三類全收斂成路徑可尋，比世界篇 §十一「協議＝檔案」更進一步）；②階段二 OS-as-game 建其上（類 Godot 節點樹）。**時間軸函數化**：空間＋複雜度由路徑直接統合，時間靠 **tick＝狀態轉移函數**外掛＝`tick(當前分佈, 使用者影響) → 下個分佈`，且可帶 Δt（時間跳躍）；代價＝**忽略 CPU 週期**（明確接受、只適用慢世界）。**已知裂縫**：Lisp `list` 數字 index vs 檔案系統字串 index（實作細節延後；Claude 提 alist/hash-table 半邊已有解）。**疑點待追認**：兩階段定案為修訂 §一／「歸一於路徑」收束 0718 呼叫閘道＋process-as-file／Godot 類比的動靜態 gap／Δt 單位（回合數 vs 模擬時鐘）／使用者影響是輸入參數 vs 執行期介入。全文見 [notes/20260719-1301-兩層底座重解-歸一於路徑-tick即時間轉換.md](workflows/notes/20260719-1301-兩層底座重解-歸一於路徑-tick即時間轉換.md)。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

## 不屬任何工作流的進度

- （無）
