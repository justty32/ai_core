# Coding Agent 圓桌 Round 1：主持人收斂

> 把四份發言收斂成一份**可開工的 coding agent v0 草案**，調和唯一的張力點。

## 0. 一句話結論

四位幾乎零分歧，核心命題完全對齊：**ai_core 式 coding agent = 一棵預先寫死的確定性組合子樹，LLM 只在葉子上「填洞 / 貼標籤」，迴圈終止由確定性謂詞（不是 LLM）持有。** 用 SSA 的話：「**我們沒有把方向盤交給乘客。**」而且——agent 不需要新執行模型，它是 `chain.py` 的超集；零件（compose 組合子 + ATP v0 模組 + idea.py LLM 原子）**全造好了**，v0 是組裝不是發明。

## 1. 已凍結的共識（無人反對）

| # | 共識 | 背書 |
|---|------|------|
| G1 | **agent 不是循環，是固定骨架 + 有界 guard/retry 迴圈**。控制流外化成確定性 Python 組合子樹（pipe=順序、route=分支、retry=有界迴圈、decompose=遞迴下降），不藏在模型腦裡 | 四人 |
| G2 | **終止由確定性謂詞持有**：`retry_until_valid` 的 `validate()` 布林 + `retries` 計數決定停或續；**LLM 只產候選、對迴圈無投票權**，它永遠等不到那個問它「要不要再來一輪」的提示詞 | SSA+AIRE |
| G3 | **LLM 留白只有兩處**：① 意圖→結構化 JSON（介面①翻譯層）② 受約束生成 body（介面②模糊前沿）。其餘全確定性。route 的 selector 可為 LLM classify，但 table 確定性+有 default | SSE+SSA+AIRE |
| G4 | **智能四分：規劃✗ / 選工具△ / 執行✓ / 反思✗**。只有「執行（受約束填洞）」交給笨模型；規劃固化成聰明模型生的任務骨架庫、選工具壓成 route 固定枚舉、反思換成確定性 guard | AIRE，全員認同 |
| G5 | **agent 本身是一個 text→text + --metadata 的 CLI 函式（遞迴閉合）**：組合子封閉在 `Fn=str→str` 型別下，agent 套 agent = 函式套函式，可進 hub/SFC、可被父 agent 當 stage | SSA |
| G6 | **agent 迴圈本身不持有危險能力**，只持有「呼叫已宣告 metadata 的函式」的能力；閘門由被呼叫函式自己宣告、dispatcher/sandbox 在呼叫點強制 fail-closed。**agent 不能自己給自己發鑰匙** | SGA |
| G7 | **v0 正常終態 = 一份帶 certificate 的待審 diff 提案**（不直接落盤、不自由跑命令）；落盤/跑測試需人類或聰明模型授權（＝發證書） | SGA |
| G8 | **驗證器三級疊加、皆與 LLM 無關**：ast.parse（語法）→ json schema/簽名比對（結構）→ subprocess 跑測試（行為）。retry 預算 = 小硬上限（retries=3，沿用 max_rounds），用盡走 on_exhausted 降級。**避免 actor-critic（笨模型驗笨模型＝方差疊方差）** | AIRE |
| G9 | **直接複用 ATP v0 四塊**：skeletonize（裁 context）/ line_assistant（錨點插入）/ isolation（軟隔離 smoke）/ asset+trace（溯源），連降級切 Ollama 都是既有 on_exhausted | SSE |

## 2. 統一的 coding agent v0 結構

融合 SSE 的 9 節管線 + SSA 的組合子樹形態 + AIRE 的智能四分 + SGA 的治理閘門：

```
coding-agent  （一個 ai_core 函式：text→text + --metadata；未認證 nondeterministic:true）
控制流持有者 = 下面這棵確定性組合子樹（compose.py），不是 LLM
─────────────────────────────────────────────────────────────────
 task(text) ─► pipe(
   [1] parse-intent          ★LLM留白① 意圖→任務JSON   guard(validate-json)
   [2] skeletonize           確定性【ATP】裁 context、標 <FILL> 洞
   [3] build-prompt          確定性  template 夾約束
   [4] guard(retry(          ─────── 有界迴圈（≤3 圈，謂詞持有終止）
         gen-code,           ★LLM留白② 在 <FILL> 受約束生成
         validate = ast.parse ∘ 簽名比對 [∘ smoke],   ← 確定性三級驗證器
         retries = 3,
         on_exhausted = 降 tier 切 Ollama / 明確失敗))
   [5] line-assist insert    確定性【ATP】錨點精準插入 → 編輯後內容(text)
   [6] smoke(isolation)      確定性【ATP】env 白名單軟隔離跑固定 import/呼叫
   [7] commit-asset          確定性【ATP】append 站到 asset JSON + certificate
 ) ─► stdout：帶 certificate 的待審 diff 提案
                          （落盤 / 跑完整測試 = 另需授權發證書）

LLM 留白①②  ──全部經──► 元件1 Entry Manager（佇列+consume rate+證書）
route table 來源 ─────► 元件4 Hub（有界 skill 清單）
資產固化 ────────────► 元件5 SFC store
trace.span 包整棵樹 ──► ATP TRACE_ID 接力棒
```

**agent 的九軸 metadata**（SSA）：`lifecycle:one_shot`、`state:stateless`（動目錄則 stateful_external+state_dirs）、`guarantee:idempotent`（若 guard 夠強——這是 agent 比 raw LLM 可靠的憑證來源）、`nondeterministic:true`（未認證）→ 過 ATP certificate 後升級成 `{model,test_set,stability}` dict 證書、**可被 hub 讀、可撤照**。

## 3. 唯一需調和的張力：smoke 跑測試（SSE 節點 vs SGA 紅線）

- **SSE** 管線節點 [6] 直接跑 smoke（isolation subprocess import/呼叫）。
- **SGA** 紅線①：跑測試是 exec 高危、**預設不准**、需 exec 證書，且「絕不接受 agent 自由組裝的 shell 字串」。

**主持人裁決（兩者其實不衝突，釘清界線）**：
1. **受控 smoke 在 v0 允許**——因為它跑的是**寫死的固定命令**（`python -c "import X; X.fn()"` 級別）、在 **ATP 軟隔離（env 白名單 subprocess）內**、對**隔離副本**而非原始 workspace。這正符合 SGA 自己的例外條款「exec 只准跑白名單內、已宣告 metadata 的固定命令」。
2. **紅線守的是「agent 自由組裝 shell 字串」和「對原始 workspace 落盤」**——這兩個 v0 一律禁止，需授權。
3. 落地對應：SSE 節點 [5] 產出的是**編輯後內容(text)**、節點 [6] smoke 跑在**隔離副本**、節點 [7] commit 是 append 到 asset JSON——**全程沒有真的寫回原始檔**。所以 SSE 管線本來就是「出提案」，與 SGA 的「預設只出 diff」**天然一致**，只需把「smoke 用固定白名單命令、跑隔離副本」寫進規格即可。

> 結論：smoke 保留在 v0 管線（它是 ATP v0 既有節點），但明文約束為「固定命令 + 隔離副本 + 不碰原始 workspace」。真正的 exec 紅線（自由 shell、落盤、裝套件、碰網路、改框架自身）維持禁止。

## 4. 行動項（四人交集，最省力的落地路徑）

1. **把 `chain.py` 的 spec schema 從 flat `stages` 升級成遞迴組合子節點** `{op: pipe|guard|retry|route, ...}`（SSA 行動項）——它就是 v0 的 **agent runner**，零新執行模型，複用 compose + trace + compose_meta（A4 推導讓 agent 九軸從成員自動算）。
2. **拼出 demo `try_implement/demos/coding_agent_v0.py`**（SSE）——節點 1/3/5/6 現成、節點 4 是 idea._stage_fn 換 system prompt，用 compose.pipe 串、guard/retry 包 [4]。唯一新寫：節點 2 的「簽名/call_site 比對」validator。
3. **與 ATP v0 開工次序合流**：ATP 的 skeleton/line_assistant/isolation/asset/trace 本就要先寫，coding agent demo 是它們的第一個真實組裝者——兩條線共用同一批底層檔，建議**一起做**。

## 5. 怎麼證明它比 raw 笨模型強（AIRE 對照實驗，凍結為驗收方法）

- 固定 N 個帶 ground-truth + 可跑驗收測試的真實 coding 任務（配對錨）。
- 兩臂配對跑：**A=raw 笨模型**（餵全檔+指令、單次、無鷹架）vs **B=ai_core 鷹架**（skeletonize→bind 填洞→guard+retry(3)→降級）。
- **主指標＝任務通過率**；**配對統計＝McNemar test**（測「B 過 A 不過」是否顯著多於反向）。
- 輔助：context token 省幅、retry 分佈、降級率。pivot 維度 `tier × method × task_type`。
- **命題**：贏的不是模型，是「**被壓到最小的決策空間 + 外接理智（確定性驗證器）**」。

## 6. 下一步

- **Round 2（可選）**：敲定 agent runner 的組合子節點 spec schema（§4.1）與 demo 的確切任務集（§5 的 N 個任務）。
- 或**直接開工**：§4 三項與 ATP v0 共用底層，可在「ATP v0 開工」時一併拼出 coding_agent_v0 demo——這會是飛輪的第一次真實轉動。

---
*產出方式：四份發言由內部並行 subagent 獨立扮演四位專家產生；收斂由主持人（Claude）撰寫。本輪未動 `_core.py` / `core_nature/`。*
