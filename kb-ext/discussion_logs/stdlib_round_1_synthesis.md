# Stdlib 圓桌 Round 1：主持人收斂

> 把四份發言收斂成一份**可開工的 stdlib v0 草案**，標出唯一的實質分歧。

## 0. 一句話結論

四位高度收斂：**stdlib 不是 CLI 工具箱，是「原子 + 組合子」的代數**。基礎函數分三層——**確定性原子（pure）/ LLM 原子（llm）/ 組合子（combinators）**——其中組合子是 ai_core 區別於 coreutils 的靈魂（「能力跳一階全在這裡」）。而且**大半已經實作**：`idea.py` 有 4 個 LLM 原子、`compose.py` 有全套組合子，v0 的工作主要是**提煉與 CLI 化**，不是從零寫。

## 1. 已凍結的共識（無人反對）

| # | 共識 | 背書 |
|---|------|------|
| S1 | **三層分類**：確定性原子 / LLM 原子 / 組合子（SSA 細分五類，本質相同：I/O邊界+純文字=確定性，LLM-binding=llm，combinator+控制流=組合子） | 四人 |
| S2 | **五個核心組合子優先**：`pipe` / `guard` / `retry` / `route` / `decompose`，先 CLI 化它們才證明 stdlib 不只是工具箱 | SSA+AIRE 點名，SSE 不反對 |
| S3 | **guard/retry 是飛輪物理基礎**：確定性驗證器（ast.parse/json.loads/schema）＝笨模型的「外接理智」，把「偶爾對」變「保證對或明確失敗」 | AIRE 提，全員認同 |
| S4 | **粒度＝coreutils 等級**：一個原子一個及物動詞，原子內部「零開放決策」，組合複雜度外包給組合子，**拒絕在原子內長 DSL/flag 矩陣** | SSA+AIRE |
| S5 | **組合子收「函數的引用」（路徑/名字/--spec），不收 inline code**——笨模型擅長選名字填空，不擅長即興寫 lambda；也讓組合圖全程可被 hub 掃描 | SSA |
| S6 | **簽名協議**：預設 `text→text`；需結構時 JSON 信封 `{ok,data,error}`；**錯誤一律 exit code+stderr，絕不混進 stdout** | AIRE 提，與既有 Layer4 錯誤封套一致 |
| S7 | **危險四級閘門**：pure/fs-read 開、fs-write 申報(`dry_run`)才開、exec/llm 領證書才開、**net v0 暫不收**。四把鑰匙全是現成（路徑白名單 / dry_run+guarantee / ATP沙箱+exec證書 / 第九軸+consume rate） | SGA |
| S8 | **md→HTML 確定性、不碰 LLM**，且是「一次性聰明模型生 renderer 資產、笨模型只呼叫」的飛輪典型範例 | AIRE 提，SSE 同意（最小子集） |
| S9 | **大量提煉而非新寫**：clean/notes/critique/expand 從 idea.py 提煉；pipe/retry/guard/fanout/route 從 compose.py 提煉。runtime 設施（memoize/recovery/interact/singleton）**不是 atom**，不入 stdlib | SSE+SSA |

## 2. 合併去重後的 stdlib v0 清單

> 來源欄：**提煉**＝現有 code 已實作、CLI 化即可；**新做**＝要寫但小；**緩**＝共識延後。

### A. 確定性原子（pure）— hub 預設全暴露，笨模型不該碰內容只該呼叫
| 名稱 | 職責 | 來源 |
|---|---|---|
| `extract-json` / `extract-code` | 從 LLM 噪音輸出撈第一個合法 JSON / 抽 ```fenced``` 區塊 | 新做（極高頻收尾） |
| `validate-json` / `validate-py` | `json.loads`/`ast.parse` 回 ok/err — **guard/retry 的驗證器來源** | 新做（小） |
| `json-array` | 把條列正規化成嚴格 JSON array of strings（含 validate 模式） | 新做（WF2 逼出） |
| `lines` / `slice` | 取行號區間 / 字元區間 — **行數助手底座**（連 ATP v0） | 新做（小） |
| `split` / `join` | 依分隔符/行/段 拆與併 — 組合子 decompose/fanout 的餵料與歸併 | 新做（薄） |
| `template` | 套 `{{var}}` 模板（穩定 prompt/few-shot 組裝） | 新做（薄） |
| `cat-files` | 一串檔路徑 → 串接 text（加來源標頭）— WF3 搬運端 | 新做 |
| `md2html` | markdown 最小子集 → HTML（標題/段落/清單/行內code/連結，**非完整 CommonMark**） | 新做（守 KISS） |

### B. LLM 原子（llm）— 全體 `nondeterministic:true`，未認證；經 Entry Manager 走 consume rate
| 名稱 | 職責 | 來源 |
|---|---|---|
| `llm` | 裸 llm_call，stdin→LLM→stdout，`--system/--prefix/--suffix` 疊 context — 所有 llm 原子的底 | 提煉（bind 已實作） |
| `clean` | 口語逐字稿 → 通順文字（WF1） | 提煉自 idea.py |
| `notes` | 整理 → 匯總筆記，衝突標 ⚠️ 不下定論（WF1） | 提煉自 idea.py |
| `critique` | 找漏洞 / challenge（WF2） | 提煉自 idea.py |
| `expand` | 擴展靈感（WF2） | 提煉自 idea.py |
| `classify` | 文字 → 單一標籤（固定枚舉選一）— **route 的 selector，把判斷與執行拆開** | 新做（AIRE 新增） |

### C. 組合子 combinators（composite）— 靈魂，能力跳階
| 名稱 | 職責 | 來源 |
|---|---|---|
| ★ `pipe` | 順序串接，前一個 stdout→下一個 stdin（四 workflow 主拼裝動作；composite metadata 由 compose_meta 推導） | 提煉（chain.py 已起頭） |
| ★ `guard` | fn 輸出過 validate 才放行，否則交 repair（可為另一次 LLM） | 提煉自 compose.py |
| ★ `retry` | 拒絕採樣，重抽到通過 validate（mretry 已強制被包者 ≥ idempotent） | 提煉自 compose.py |
| ★ `route` / `branch` | 用 classify 算 key 分派固定分支（switch.py 組合子化） | 提煉自 switch.py |
| ★ `decompose` | split→各塊過同一 sub→join（分而治之，大任務拆小） | 提煉自 compose.py |
| `with-fallback` | 主函式失敗/不合格 → 備援（降級鏈） | 提煉 |
| `fanout` / `reduce` | 同輸入餵多分支 → list / 多份歸併成一 | 提煉（WF②③用） |
| `vote` / `best-of` / `actor-critic` | self-consistency / scorer 取最佳 / 生成↔批評來回 | **緩**（N 倍呼叫，窮專案奢侈品，留 lib 不升 CLI） |

### D. I/O 邊界與危險原子（按 SGA 分級，預設不暴露給笨模型）
| 名稱 | 危險級 | 最小治理 | v0 取捨 |
|---|---|---|---|
| `read-file` / `ls-files` | fs-read | 路徑限 workspace 根、不跟隨 symlink 出界 | 收（但 SSE 提醒：shell `cat` 已涵蓋大半，只補 `cat-files`） |
| `write-file` / `append-file` / `tee` | fs-write | `dry_run:true`+`guarantee:transactional`+路徑白名單 | 收，**預設不入笨模型拼裝池** |
| `sh` / `exec-shell` | exec（最高危） | ATP 三層 AST fail-closed + env白名單軟隔離 + 領 exec 證書 + 預設不暴露；**永無無護欄版** | 收，重護欄 |
| `fetch-url` | net | 域名白名單+唯讀GET+預設關 | **v0 不收**（等 workflow 逼出） |

## 3. 唯一的實質分歧：`gather` 該不該用 LLM？

- **AIRE**：gather 是純資料搬運（JSON `{files,text}`→拼 markdown），**該確定性、零 LLM**，讓 LLM 碰只引入幻覺、毫無收益。
- **SSE / SGA**：gather 含「彙整＋處理衝突＋成 single-truth」，需要判斷，是 **LLM atom**（prompt 複用 notes 的衝突語意）。

**主持人裁決建議（拆一刀，兩邊都對）**：把 gather **拆成兩個原子**——
1. `cat-files`（pure）：把 `{files,text}` 機械串接成單一 text（AIRE 對：搬運零 LLM）。
2. `merge`（llm，複用 notes 衝突 prompt）：對串接後的 text 做「衝突彙整成 single-truth」（SSE/SGA 對：判斷需 LLM）。
WF3 ＝ `cat-files | merge`，正好示範 S1 的三層拆分與 S6 的管線協議。**「gather」不再是一個原子，而是一條 2 節 pipe。** ← 待四位下一輪確認或推翻。

## 4. 優先序（建議）

1. **先寫確定性骨頭**（A 類的 validate-* / extract-* / lines / slice）——它們是 guard/retry 的驗證器來源，沒有它們組合子是空的。
2. **CLI 化五個核心組合子**（pipe/guard/retry/route/decompose）——從 compose.py 提煉，S2 共識。
3. **提煉 idea.py 的 5 個 LLM 原子**（含新增 classify）。
4. **補 WF 專用**：cat-files / merge / json-array / md2html。
5. **緩**：vote/best-of/actor-critic（留 lib）、整套 fs 工具箱、net、完整 md parser。

## 5. 下一步

- **Round 2（建議）**：四位確認 §3 的 gather 拆分裁決，並把 A 類確定性原子的**確切簽名**敲定（這批是新做、要先定契約）。
- 或直接挑 §4 第 1-2 項開工（與 ATP v0 的 skeleton/lines 有重疊，可一起做）。

---
*產出方式：四份發言由內部並行 subagent 獨立扮演四位專家產生；收斂由主持人（Claude）撰寫。本輪未動 `_core.py` / `core_nature/`。*
