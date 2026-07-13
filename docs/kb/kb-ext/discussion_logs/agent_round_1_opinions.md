# Coding Agent 圓桌 Round 1：用 stdlib 拼一個簡單的 coding agent

> 主題：**如何用 ai_core 的 stdlib（原子+組合子）+ ATP v0，拼出一個簡單的 coding agent？**
> 核心張力（前提）：**ai_core 式 coding agent ≠ ReAct 那種「LLM 在駕駛座的自主 tool-calling 大循環」**。roadmap 願景「LLM 退縮到兩介面、其餘確定性程式碼」→ agent ＝確定性骨架 + 最小 LLM 留白。
> 四位專家發言由內部並行 subagent 獨立產出。

---

## SSE（最小可跑的 coding agent — 實際拼出來）

ReAct agent 的本質是「LLM 在 while 迴圈裡自己決定下一步呼叫哪個 tool」——**控制流在模型手裡**。ai_core 式 agent 剛好相反：控制流是寫死的 shell pipe / chain spec，LLM 只在管線中段被叫進來填一段留白，**填完就被踢出駕駛座**。看 `idea.cmd_ingest` 就懂這味道：raw 寫檔→clean(LLM)→notes(LLM)，站與站之間沒有「模型決定要不要再來一輪」，是 `compose.pipe` 鎖死成固定序列。

**任務選型（別貪心）**：比 ATP v0 的「插入一個函式」往上加**一格**——「在一個已有 Python 檔裡，按一句自然語言意圖，新增一個函式並接好一處呼叫點」。剛好逼出 decompose 的用處，但不碰跨檔/依賴推導/多檔。**v0 別貪**：不做自主重規劃、不做 LLM 選工具、不做跨檔、不做 vote/best_of、不做「模型寫測試再自己跑」的自我驗證迴圈。

**管線形狀**：前半確定性「上下文打包」（讀檔、AST 裁剪、標錨點），中段唯一 LLM 留白（受約束生成，只准吐一個函式 body），後半確定性「驗證與落地」（ast.parse 守門、line_assistant 精準插入、subprocess 隔離冒煙）。**guard/retry 只包在 LLM 那一站外面——馴化只施加在唯一會吵的節點上。**

```
任務描述(text)  e.g. "加一個 slugify(text)，並在 save() 裡呼叫它"
  ▼
[1] parse-intent ───────── ★LLM 留白#1（意圖翻譯層，介面①）
  │  out: 任務 JSON {new_fn, signature, call_site, anchor}
  │  包 guard(validate=validate-json)：吐不出合法 JSON 就 retry
  ▼
[2] skeletonize ────────── 確定性【複用 ATP skeleton.py】只留簽名/錨點，body 砍成 ...
  ▼
[3] build-prompt(template)─ 確定性  夾「只准輸出一個函式，簽名須為 X」
  ▼
┌─ guard( retry( ───────── ★LLM 留白#2（模糊前沿，介面②）
│ [4] gen-code ──────────  唯一的「寫碼」LLM
│ [5] extract-code ──────  確定性（剝 ``` 圍欄/廢話）
│ [6] validate-py ───────  確定性 ← retry 判定條件（語法過? 簽名符合?）
│ ), retries=3, on_exhausted=切 Ollama 重跑 )  ← ATP 降級分支
└─ 出來保證是「語法 OK 的一個函式」(text + cert:syntax_ok)
  ▼
[7] line-assist insert ─── 確定性【複用 ATP line_assistant.py】錨點精準插入；產出編輯後內容(text)
  ▼
[8] smoke(isolation) ───── 確定性【複用 ATP isolation.py】env 白名單軟隔離跑 import/呼叫
  ▼
[9] commit-asset ───────── 確定性【複用 asset.py+trace.py】append 一站到 asset JSON
```

**四個指定點**：(1) **LLM 留白只有兩處**（[1] 意圖→JSON、[4] 受約束生成 body），其餘 7 節全確定性；(2) guard/retry 只包 [4]–[6]；(3) 直接複用 ATP v0 四塊：skeletonize/line_assistant/isolation/asset+trace，連降級切 Ollama 都是既有 on_exhausted；(4) **比 raw 笨模型多了四道確定性護欄**——skeleton 收斂 context（少幻覺）、ast.parse 擋語法錯（保證 syntax_ok 才動檔）、line_assistant 讓編輯只發生在錨點（不波及無關行）、isolation smoke 給「至少能跑」的證據。模型還是那個笨模型，但**錯誤被確定性地攔截或重抽掉了**。

**落地**：節點 1/3/7/8 全現成，節點 4 是 `idea._stage_fn` 換 system prompt，用 compose.pipe 串、guard/retry 包中段——**今天就能拼 demo** `try_implement/demos/coding_agent_v0.py`，跟 v0_pipeline.py 並列。唯一新寫的是節點 2 的「簽名/call_site 比對」validator。**agent = 把既有積木用組合子鎖成一條死管線，LLM 只是其中兩個被夾緊的格子。**

---

## SSA（agent 架構形態 — 三個架構決策）

讀完 compose/chain/hub/sfc/_core，判斷很明確：**agent 需要的零件全造好了，只是還沒人按 ai_core 哲學排成形狀。** compose 的 `pipe/guard/retry_until_valid/route/decompose/with_fallback` 就是 agent 控制流的全部語彙。ReAct 需要「LLM 決定下一步」的大循環，是因為控制流藏在模型腦子裡；我們把控制流**外化成確定性 Python 結構**——pipe 是順序、route 是分支、retry 是有界迴圈、decompose 是遞迴下降。**控制流外化後，agent 不是循環，是一棵預先寫死的組合子樹，LLM 只在葉子上。**

關鍵洞察在型別簽名：每個組合子吃 `Fn = Callable[[str],str]` 回傳 `Fn`，**agent 組裝出來的東西型別上和一個 atom 完全一樣**——都 text→text。chain.py 已示範把這代數搬到 shell 且自己 register+intercept，是合格 ai_core 函式。把 chain 的 spec 從「線性 stages」擴成「組合子樹」，它就直接變 agent runner。**agent 是 chain 的超集，不需要新執行模型。**

**結論一：agent 是固定骨架 + 有界 guard/retry 迴圈，終止謂詞由確定性程式碼持有。** 傳統 agent 的「不定長」來自把「要不要再來一輪」交給 LLM；我們奪回它。看 `compose.py:125 retry_until_valid`：`for _ in range(max(1,retries))` 是確定性次數上界，`validate(last)` 是確定性成功謂詞。**LLM 只負責 fn（產候選），完全沒有迴圈控制權**——它不能說「再給我一輪」也不能說「夠了停」。在 coding 場景 validate 就是 ATP guard（compile/pytest/linter），**確定性辦得到的驗證絕不交給 LLM 自評**。控制流持有者：compose 組合子（純 Python），不是 LLM。**這是和 ReAct 的分水嶺。**

**結論二：是，agent 就是一個實作 --metadata 的 text→text CLI 函式——遞迴閉合。** 因為組合子封閉在 Fn 型別下，agent 組裝完還是 str→str，天生能包成 CLI 函式。**agent 套 agent = 組合子套組合子 = 函式套函式，沒有第二種機制。** 子 agent 對父 agent 是 pipe 裡一個 stage；agent 進 SFC = 變成 store 裡一個 tiny function。hub 掃描時 agent 和 atom 沒區別——這是飛輪「資產被當原子再組合」的兌現。九軸 metadata：
- `lifecycle: one_shot`（骨架跑完即退，多輪是內部的事，對外仍一次 text→text）
- `state: stateless`（要動工作目錄則 stateful_external + state_dirs）
- `guarantee: idempotent`（若 guard 夠強——有 validate gate，可安全重試，這是 agent 比 raw LLM 可靠的憑證來源）
- `nondeterministic`：**第九軸主場**。未認證 agent ＝ `true`；過了 ATP certificate 流程（夠穩測試組+穩定度%）→ 升級成 dict 證書 `{model,test_set,stability}`。**agent 的「可不可信」不是形容詞，是 metadata 裡一個可被 hub 讀、可撤照的欄位。**

**結論三：骨架自己持有控制流（絕不下放）；元件 1/4/5 是外設、2/3 是構成材料。**
- **元件1 Entry Manager**：所有 LLM 留白點的唯一出口（佇列+consume rate+證書），retry 重抽成本由它計量。
- **元件4 Hub**：agent 啟動時「我能用哪些工具」的來源（有界 skill 清單），塞進 classify/route prompt。**hub 給清單，選哪個仍由 route 的 selector 持有**（selector 可為 LLM classify，但分派表是確定性的）。
- **元件5 SFC**：agent 固化小工具/小 agent 的倉庫，也是 agent 自己被收編的地方。
- **元件2+3**：不是外設，是骨架的構成材料（元件2 把 llm_call 疊成語意葉子，元件3 是每個 stage 的封裝形式）。

**行動項**：把 chain.py 的 spec schema 從 flat stages 升級成遞迴組合子節點 `{op: pipe|guard|retry|route, ...}`，它就是 v0 的 agent runner——零新執行模型，複用 compose+trace+compose_meta（A4 推導讓 agent 九軸從成員自動算）。**「我們沒有把方向盤交給乘客。」**

---

## SGA（agent 治理 — 把自主循環關進閘門）

先把張力擺正：**「coding agent」在我這位子上不是產品名詞，是威脅模型名詞。** ReAct 自主大循環是治理最壞情況，因為它把「決策權」和「危險能力」綁在同一個非決定性主體上。我們在 ATP v0 和危險四級建的閘門，本質都在做同一件事：**把「決定要做什麼」和「真的去做」拆開，中間插一道證書閘門。** 所以 ai_core 式 coding agent 的定義該倒過來寫——**它不是會自己動手的迴圈，是一個「生成待審提案」的管線。**

關鍵設計：**agent 迴圈本身不持有任何危險能力，它只持有「呼叫已宣告 metadata 的函式」的能力。** 危險能力住在被呼叫的 stdlib 函式裡，那些函式的 --metadata 已誠實標好自己是 fs-write/exec/llm（如 idea.py 各子命令老實宣告）。**閘門不是裝在 agent 上的——閘門是每個函式自己宣告、由 dispatcher/sandbox 在呼叫點強制。** agent 想越權唯一的路是去呼叫未領證書的高危函式，而那呼叫會在 fail-closed 護欄處直接被拒。**危險能力預設不在笨模型搆得到的桌面上，agent 連調用都失敗，不是調用後才補救。**

一輪迴圈：讀檔(fs-read)、驗證(pure) 是開的（迴圈的眼睛）；裁剪(prune) 走 ATP AST 三層護欄、fail-closed；打 LLM(llm) 必須領第九軸證書才准進迴圈、受 consume rate 守門。然後兩條紅線格：**寫檔(fs-write) 預設只准產出 dry_run 的 diff 提案、不准落盤；跑測試(exec) 預設不准。** **agent v0 一輪的正常終態，是吐出一份帶 certificate 的待審編輯提案**（diff + 過了哪些 guard + 哪個模型生的 + sandbox 等級 + status），落盤與跑測試需人類/聰明模型授權後才執行（授權形式＝發一張 exec/fs-write 證書貼到 asset 上）。

證書記四件事：model（哪個笨模型）、guards_passed（過哪幾層 AST 護欄）、sandbox_level、status。**迴圈每轉一圈 status 只能單調往「更被信任」或「被拒」走，不能由 agent 自己往上跳格。** 撤照觸發：穩定度掉到閾值下、retry 耗盡、guard 事後加嚴。**證書是可撤的租約，不是一次性通行證；agent 的自主性是被租給它的，隨時收回。**

| agent 一輪動作 | 危險級 | 預設准否 | 閘門/鑰匙 | 證書要求 | 違反後果 |
|---|---|---|---|---|---|
| 讀檔 fs-read | 低 | ✅ 開 | 路徑白名單 | 無 | 越界拒讀 |
| 裁剪 prune | 中(已內化) | ✅ 開 | AST 三層 fail-closed | 無 | cert.status=rejected{guardrail_violation} |
| 打 LLM 生碼 | 高(非決定+帳單) | ⚠️ 限 | 第九軸+consume rate | 未認證→true、受 rate 守門 | 超 rate 即停；無證書只能標隨機 |
| 驗證 pure | 低 | ✅ 開 | 無副作用 | 無 | — |
| 寫檔/套用編輯 | 中 | ❌ 預設只出 diff 提案 | dry_run+guarantee+路徑白名單 | 落盤需 fs-write 證書 | 落框架自身/越界即 reject |
| 跑測試 exec | 高 | ❌ 預設不准 | ATP 沙箱+exec 證書 | 需 exec 證書、命令限白名單固定 runner | 任意 shell 拒；撤照即收回 |

**v0 紅線清單（agent 永不自決，連發證書的路都不開）**：① 跑任意 shell（exec 只准白名單內已宣告 metadata 的固定命令，絕不接受 agent 自由組裝的 shell 字串）；② 改 stdlib 自身 / ATP 護欄 / _core.py（馴化框架不能被被馴化者改寫）；③ 寫出 workspace 根之外；④ 安裝套件/觸碰網路；⑤ 自己發證書/自己升 status/自己撤別人的照。**v0 coding agent 正常終態＝一份帶證書的、待蓋章的 diff。它能想、能讀、能驗、能生成提案——但「真的去改、真的去跑」這兩格的鑰匙不在它身上，且永遠不能由它自己鑄造。**

---

## AIRE（笨模型當 agent 大腦可行嗎 — 智能四分）

笨模型當 agent 大腦，**不是不可行，而是「大腦」這比喻本身要拆掉**。ReAct 把 LLM 當會持續做決策的 CPU，每 tick 重新「想一下接下來做什麼」。在 roadmap 前提下這顆 CPU 是壞的：<10B 模型每步自由決策正確率假設 0.7，8 步 ReAct 軌跡整體成功率 0.7^8≈0.058——**指數衰減是這條路線的數學宿命**。interact.py 連自家 actor_critic/run 都不信任會自然停、硬塞 max_rounds 安全閥——一個連自家組合子都要外接強制終止的系統，絕不該把「下一步做什麼」交給笨模型在大循環裡擲骰。**正確設計反過來：把規劃與反思固化成確定性骨架，讓笨模型只在骨架預留的、零開放決策的洞裡填碼。**

**智能四分配表（整個設計的骨架）**：

| 智能成分 | 笨模型行不行 | 誰來做/怎麼接管 | ai_core 零件 |
|---|---|---|---|
| **規劃**（任務分解、步驟序） | ✗ 最弱項，長程必崩 | **固化成確定性骨架**。coding 任務步驟序有限可枚舉，由聰明模型一次性生「任務骨架庫」當耐久資產，笨模型消費。 | compose.pipe/decompose |
| **選工具**（呼叫哪個函式） | △ 只有壓成「固定枚舉選一」才行 | **降級成 classify**：從 N 個預定標籤選一個，選錯有確定性 fallback。 | compose.route（table 固定+default） |
| **執行**（生成具體 code） | ✓ **唯一該交給它的** | 在精準標記的洞裡做受約束語意轉換：餵 skeletonize 裁過的 context，產一小段碼。 | llm_call.bind + ATP skeletonize |
| **反思**（發現錯、自我修正） | ✗ 「let me reconsider」是聰明模型才有的 | **用確定性 guard 取代**：靠 ast.parse/跑測試/json schema 這種外接理智判對錯，錯了帶機器產生的具體理由重抽。 | compose.guard/retry_until_valid |

**四項裡只有一項（執行）真交給笨模型，其餘三項全被確定性程式接管或壓縮。** 笨模型在這條 pipe 裡的角色小到近乎一個「吵的純函式」。

**最小決策壓到兩種，且各帶確定性兜底**：① 受約束生成（在 skeletonize 標記的 `<FILL>` 洞填碼，沒有「要不要動別處」的自由度，因為別處沒餵給它）；② route 的 selector（從固定枚舉如 `{add_param,fix_import,wrap_try,rename}` 選一個，一定配 default 分支，選錯走保底）。**禁止出現的決策形態**：任何「接下來該做什麼/要不要再來一輪/這樣夠了嗎」——全部上移給確定性骨架。**笨模型可以選「填什麼」，不可以選「下一步做什麼」。**

**反思迴圈整個換成 guard+retry，驗證器必為確定性、與 LLM 無關**。三級疊加：(1) 語法層 ast.parse（擋掉最常見的語法壞掉）；(2) 結構層 json schema/簽名比對；(3) 行為層 subprocess 跑測試。**這三層沒一個需要 LLM——這就是「笨模型的外接理智」**。`actor_critic` 那種「LLM 當 critic」在笨模型場景要降級或避免——**讓笨模型批評笨模型是拿不可靠驗不可靠，方差疊方差**。retry 預算沿用 max_rounds 哲學設小硬上限（retries=3），用盡走 on_exhausted（降 tier 或明確失敗），**絕不無上限重抽**（那會把省錢變燒錢黑洞）。

**對照實驗（沿用 tier×method pivot）**：固定 N 個帶 ground-truth + 可跑驗收測試的真實 coding 任務（配對的錨）。兩臂配對跑：**臂A=raw 笨模型**（餵全檔+指令，單次生成，無鷹架）；**臂B=ai_core 鷹架**（skeletonize→bind 填洞→guard+retry(3)→降級）。**主指標=任務通過率（驗收測試綠的比例）**。**配對統計＝McNemar test**（看「B 過而 A 不過」是否顯著多於反向，配對消掉任務難易干擾）。輔助：context token 省幅、retry 次數分佈、降級率。pivot 維度 `tier × method × task_type`。**意義**：若 B 顯著高於 A 且增益主要來自「ast.parse+跑測試擋掉 A 裡語法/行為錯的輸出」，就證明核心命題——**不是笨模型變聰明了，是確定性骨架把它的決策空間壓到小到不會出錯，再用外接理智把它偶爾的對撈成穩定的對。**
