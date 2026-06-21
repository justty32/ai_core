# Stdlib 圓桌 Round 1：基礎函數該有哪些

> 主題：**ai_core 標準函式集（stdlib）v0 該有哪些基礎函數？** 基礎函數＝`text-in → text-out` 的原子 CLI 函式（LISP atom），笨模型靠**拼裝**它們做事。背景＝使用者 2026-06-08 口述的四條 workflow（①口述→notes ②notes→AI建議 ③gather→single-truth markdown ④md→HTML）與「make a standard tool set」訴求。
> 四位專家發言由內部並行 subagent 獨立產出。

---

## SSE（務實剛需 — 從現有 idea.py 與四 workflow 倒推）

四條 workflow 攤平只有三類動作：**過 LLM 的語意轉換**（clean/notes/critique/expand/gather 彙整）、**純文字結構轉換**（md→HTML、JSON array 取出展開）、**I/O 邊界**（讀檔成 text、展開 `{files:[...]}`）。`idea.py` 已把第一類做掉一大半——clean/notes/critique/expand 全是現成 `bind(system=...)` 包的 `text→text` filter，**直接就是 stdlib atom**，只差從 dispatcher 提煉成獨立函式。WF1/2/4/5 其實已能跑，缺的是**兩側的搬運與成形函式**。

真正缺口在 WF3（gather）：輸入 `{files,text}` 逼出 `cat-files`（atom/fs）；彙整＋衝突處理本身是 LLM atom，**prompt 跟 `notes` 同源**（notes 已寫「衝突標 ⚠️、不下定論」）可共用。WF2→WF4 那句「輸出 JSON array of strings」點名一個反覆需要的 pure atom `json-array`（把 LLM 吐的條列正規化成嚴格 JSON），配 `compose.retry_until_valid` 就是「逼小模型吐合法 JSON」的標準拼法。WF4（md→HTML）唯一不該碰 LLM，但踩到鐵則紅線：標準庫沒 md parser——**別自己寫 CommonMark**，v0 先做只認標題/段落/清單/行內code/連結的最小子集 `md2html`，註明非完整。

**最小可用集**：
- 提煉自 idea.py（最便宜）：`clean`/`notes`/`critique`/`expand`（atom,llm）、`llm`（裸 llm_call，所有 llm atom 的底）
- WF3 新做（小）：`cat-files`（atom,fs）、`gather`（atom,llm，複用 notes 衝突語意）
- 馴 LLM 輸出（最該補）：`json-array`（atom,pure，含 validate 模式）、`md2html`（atom,pure，最小子集）

**別現在做**：組合用 CLI（compose.py 全做好了，拼裝交 chain/shell pipe）；完整 md/HTML parser；把 memoize/recovery/interact/singleton 當 atom（它們是 runtime 設施）；整套 read/write/glob fs 工具箱（四 workflow 只逼出 cat-files 一個，shell 的 cat/重導已涵蓋其餘）。

---

## SSA（分類體系與粒度 — 把「組合子」立成支柱）

結論先釘死：**ai_core 的 stdlib 不是 CLI 工具箱，而是一套 atom + combinator 的代數**。工具箱（coreutils/jq/pandoc）給「會做事的指令」，笨模型得自己想怎麼接管線/重試/分支——正是它最不會的。ai_core 的差異化全壓在組合子：把「怎麼接、怎麼救、怎麼分」也變成可宣告、有 `--metadata` 的一等公民。沒這類，stdlib 退化成又一個工具箱。

**五類分層（邊界→內容→智能→結構→流程）**：
1. **I/O 邊界（fs/exec）**：唯一允許副作用，必標 `state:stateful_external`+`state_dirs`。副作用集中於此，其餘四類才能保持 stateless，讓 compose_meta 推導有乾淨輸入。
2. **純文字轉換（pure）**：`text→text` 確定性原子，`guarantee:idempotent`、`nondeterministic:false`。LISP car/cdr/cons 等價物。
3. **LLM-binding（llm）**：**全體強制 `nondeterministic:true`**，第九軸落地。idea.py 四子命令即範本。
4. **組合子 combinators**：吃「函數的引用」吐「新函數行為」，靈魂。
5. **控制流（pure, meta）**：retry/guard/validate/route——馴化第 3 類隨機性的專用工具，獨立成類＝把「LLM 輸出要先驗再用」的紀律寫進結構。

**原子 vs 複合的界線畫在「副作用與謂詞」，不畫在行數**：atom＝行為不可再用 stdlib 內其他函數表達者（所有 I/O、純文字原子、llm-call 基底、**及所有組合子本身**——組合子作為機制是 atom，它們*生產* composite）。composite＝由 atom 經組合子拼出、metadata 應**可從成員推導**者。**這條界線直接定義 A4 範圍**：A4＝「composite 的 metadata 可由成員 atom 自動推導」，compose_meta.py 已證 pipe/fanout 的 guarantee/state 可推；atom 手標、composite 一律推導、推不出的軸誠實留白讓 hub 看見缺口。

**粒度定在 coreutils 等級，一個原子一個及物動詞，組合複雜度外包給組合子而非塞進單一原子的 flag 矩陣**。switch.py「純資料 rule list、無 DSL/eval」是對的方向。關鍵：**combinators 收「函數的引用」（檔路徑/SFC名/--spec JSON），不收 inline 程式碼**——笨模型擅長填空選名字，不擅長即興寫 lambda，這樣整個組合圖才全程可被 hub 掃描、可推導。

清單（節選核心）：第1類 `cat-file`/`write-file`/`glob`/`run`/`tee`；第2類 `split`/`join`/`slice`/`replace`/`match`/`template`/`wrap-md`/`wrap-html`/`count`；第3類 `llm-call`/`bind`/`clean`/`notes`/`critique`/`expand`/`extract`；**第4類 `pipe`/`fanout`/`reduce`/`decompose`/`route`/`with-fallback`**；第5類 `validate`/`retry`/`guard`/`vote`/`best-of`。
**優先級**：先把第4類 CLI 化做到能跑通四 workflow（②③尤依賴 fanout/reduce/route），這類證明出來 stdlib 才不只是工具箱。`vote`/`best-of` 緩做（N 倍 LLM 呼叫，窮專案成本最高）。

---

## SGA（權限/危險分級 — 哪些基礎函數需要治理）

一句釘牆上：**stdlib 的危險不在「函數會不會出錯」，而在「笨模型會不會被框架默許去做它根本不該做的事」。** 純函式錯了重跑就好；但寫檔/執行外部/打 LLM 一旦預設開著，一個拼裝失誤就是不可逆破壞、無上限帳單、或沒簽核的隨機輸出溜進 single-truth。**stdlib 是第一道治理面**，每個原子出生就帶危險級和護欄。

**分級原則：非決定性走第九軸證書，副作用走 dry_run+guarantee+軟隔離，兩者正交、都要查**（別讓 exec/fs-write 的決定性危險因為「不是隨機」就逃過閘門）。

**「拒絕為預設」三條硬規則**：(1) **預設唯讀**——原子沒顯式宣告 `dry_run`/寫入語義，hub 就當它不准寫。(2) **exec 與 net 預設不入笨模型的自由拼裝池**——存在但 hub skill 清單預設不暴露，靠聰明模型生資產或人類證書點名才放行（「LLM 預設零領地」＝能力預設不在笨模型搆得到的桌面上）。(3) **LLM 原子一律 nondeterministic**，未過穩定性閘前永遠 `true`,輸出進 workflow③ 只能進「待審」不能直接落地。

**最危險三個**：`sh/exec-shell`（核按鈕）＝ATP 三層 AST fail-closed+env白名單軟隔離+預設不暴露+領 exec 證書，永不提供無護欄版；`write-file`＝`dry_run:true`+`guarantee:transactional`+路徑白名單（禁寫出 workspace 根）；`llm`＝nondeterministic+強制經 Entry Manager 走 consume rate（禁裸呼叫繞過 rate meter）。

**安全姿態總結**：Pure/fs-read 開、fs-write 申報才開、exec/llm 領證書才開、**net v0 暫不收**（等真實 workflow 逼出來，符合「目標問題＝停止鍵」）。四級閘門對應四把已存在的鑰匙（路徑白名單、dry_run+guarantee、ATP沙箱+exec證書、第九軸+consume rate），沒一把是新發明——這是 KISS 與「拒絕為預設」能同時成立的原因。

---

## AIRE（笨模型拼裝視角 — 粒度與槓桿）

笨模型拼不拼得起來，唯一決定因素是**它每一步要做的決策有多大**。笨模型每做一個自由決策就擲一次骰子，鏈越長越自由，整鏈成功率＝各環乘積——**指數衰減**。所以黃金粒度的判準是：**這個原子內部還剩多少自由決策？** 要的是「對笨模型幾乎沒有決策空間」的原子——它知道呼叫 clean/notes/to-html 各自意味什麼，但不需要它去**設計**清理規則或 HTML 模板。原子內部把「困難的開放決策」固化成確定性 code（或一次性聰明模型生的 prompt 資產），笨模型只負責「選哪顆、怎麼接管線」這種封閉可枚舉的決策。四 workflow 剛好標定甜蜜點：每條 ≈ 3-5 顆原子的 pipe。

**真正讓能力跳一階的不是更多原子，是少數幾個 combinator**。`guard`/`retry`（retry_until_valid）是質變點：笨模型輸出本質是吵的隨機函式，但只要有一個**確定性驗證器**（ast.parse/json.loads/schema/行數比對），retry+guard 把「偶爾對」變成「保證對或明確失敗」——**這是飛輪的物理基礎，驗證器是笨模型的外接理智**。第二槓桿 `decompose`（拆→各自處理→合）：把它做不到的大任務拆成做得到的小塊，是「把不可能變可能」。第三 `route`/`branch`：先用便宜 `classify` 選路再走固定分支，把「判斷+執行」拆成兩個各自簡單的決策。`vote`/`best-of`/`fanout` 錦上添花但 N 倍成本，窮專案次級。

**哪些笨模型根本不該碰**：所有純文字機械操作。**`gather`（JSON→拼 markdown）是純資料搬運，該確定性、零 LLM——讓 LLM 碰只引入幻覺、毫無收益**。md→HTML 同理確定性（標準庫沒 md parser，但這正是「一次性聰明模型生確定性 renderer 資產、之後笨模型只呼叫」的飛輪典型）。真正需要 LLM 的只有語意轉換：clean/notes/critique/expand。**預設一個函數是 pure，要標 llm 必須證明確定性 code 辦不到**。

**簽名統一是命根子**：(1) 預設 `text→text`（shell `|` 母語）；(2) 需結構時走 JSON-on-stdin/stdout，約定信封 `{"ok":bool,"data":...,"error":...}` 讓 guard 能機械判成敗；(3) **錯誤一律 exit code+stderr，絕不混進 stdout**（否則笨模型分不清結果與錯誤，管線就髒）。一句話：**stdout 永遠只有乾淨結果，成敗看 exit code，結構化才上 JSON**。

清單（★＝高槓桿 combinator）：
- 確定性原子：`gather`/`to-html`/`extract-code`/`extract-json`/`validate-json`/`validate-py`/`lines`/`slice`/`template`/`jsonl`
- LLM 原子：`clean`/`notes`/`critique`/`expand`/**`classify`**（route 的 selector 來源，把判斷與執行拆開的關鍵）
- Combinators：**★`guard` ★`retry` ★`pipe` ★`decompose` ★`route/branch`**、`with-fallback`、`vote`/`best-of`/`fanout`（次級）、`actor-critic`（選配）
**優先**：v0 先扶正 `pipe/guard/retry/route/decompose` 五顆 combinator + 全部確定性原子為 CLI，LLM 原子沿用 idea.py。vote/best_of/actor_critic 留 lib 暫不升 CLI（成熟後的奢侈品）。
