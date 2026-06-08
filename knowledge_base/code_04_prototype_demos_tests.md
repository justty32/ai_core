# code_04 — 原型 demo、煙霧測試、範例函式與 store

> **來源**：`try_implement/demos/`（reliable_code_qa.py / call_chain_trace.py / resumable_batch.py）、`try_implement/smoke_test.py`、`try_implement/lib_smoke_test.py`、`try_implement/funcs/`（upper.py / reverse.py / c_linter.sh / py_linter.sh）、`try_implement/store/`（functions.json / index.json）
> **狀態**：原型／探索（demo + 自我測試）。**測試結果已實機驗證（2026-06-08）**：83 + 78 = **161 項斷言全綠**（執行於 repo 的 `.venv` Python 3.14）。（2026-06-07 時為 72 + 68 = 140；點子捕捉軌 dogfood 與 Gap G 修復後增 21 項。）
> **一行摘要**：三個自我驗證的端到端 demo、兩套純標準庫煙霧測試（共 161 斷言）、範例函式與已 seed 的 SFC store。

相關交叉引用：被測的工具見 [code_02_prototype_tools.md](code_02_prototype_tools.md)；被測的 library 見 [code_03_prototype_lib.md](code_03_prototype_lib.md)；馴化框架見 [doc_20_taming_framework.md](doc_20_taming_framework.md)；組合維度見 [doc_21_composition_dimension.md](doc_21_composition_dimension.md)。

怎麼跑（cwd 須為 `try_implement/`）：
```bash
python3 smoke_test.py        # 工具：83 斷言
python3 lib_smoke_test.py    # lib：78 斷言
python3 demos/reliable_code_qa.py
python3 demos/call_chain_trace.py
python3 demos/resumable_batch.py
```
（兩套煙霧測試純 `__main__`，不依賴 pytest；用自訂 `check(label, cond, detail)` helper 計數，每個 `check()` = 一項斷言，跑完印「全部通過：N 項斷言」。）

---

## 三個端到端 demo

每個 demo 同時是示範**與**自我測試：跑完用 `assert` 證明每個機制如預期生效。

### `demos/reliable_code_qa.py` — 可靠程式碼問答函式

把馴化框架多層串成**一個**可靠函式，全用 `lib/` 零件 + `ScriptedBackend` 模擬 LLM 的「同輸入不同輸出」隨機性，證明整套串起來真能動。對應 [doc_20_taming_framework.md](doc_20_taming_framework.md) §5。

組裝的層（`build_reliable_qa`）：
- **L0 契約** `llm_call.bind(system=...)` — 綁定角色，要求輸出帶 ` ```python ` 區塊。
- **L2 驗證** `compose.retry_until_valid` — 沒有程式碼區塊就重抽（拒絕採樣）。
- **L2 修復** `compose.guard` — 有區塊但語法錯 → 交給 `repair`（另一次 LLM 呼叫）修。
- **L1 確定化** `memoize.Memoizer` — 同問題不重算（命中完全不碰 backend）。

mock 設計：answer backend 第一抽無程式碼（被 retry 拒）、第二抽有區塊但少右括號（語法錯，被 guard 送修）；repair backend 回正確版。

自我測試 assert：retry 應抽 2 次、guard 應觸發 1 次修復、最終語法正確、memoize 第一次 miss 第二次 hit、命中快取後 backend 呼叫數不再增加。**證明每層都生效。**

### `demos/call_chain_trace.py` — 重建 sfc forge 的調用鏈成樹

對應 `lib/trace` 落地。以 subprocess 跑 `sfc.py forge`，餵 `shout`、`wc_words` 兩個 call 請求加 shutdown，收集其 stderr，用 `trace.Collector` 把 trace 事件重組成調用樹並印出。

自我測試 assert：樹只有一個 root、root 名為 `forge.serve`、子節點含 `forge.call:shout` 與 `forge.call:wc_words`、每次 call 都有完整 start/end（complete）。

（附註：router dispatch 也用 `trace.child_env()` 把 trace id 帶給子 process——跨 process 的樹組裝需外層收集子 process stderr，同理。）

### `demos/resumable_batch.py` — 可續傳批次處理器

驗證「標準狀態目錄慣例」+「中斷恢復慣例」參考實作真能扛中斷（`lib/recovery` + `lib/state_dirs`）。批次處理 `["a","b","c","d","e"]`（範例處理=轉大寫）：成果寫 `.data/batch/results.json`、進度 offset 寫 `.state/batch/recovery/offset.json` + `recovery.json` manifest。第一次跑到 item 2 前「崩潰」（拋例外，recovery 停在 in_progress）；第二次重跑 `startup` 偵測到 in_progress + mode=resume → 從斷點接續，不重做已完成的。

自我測試 assert：第一次應崩潰、崩潰後 detect=resume/status=in_progress、第二次從 index 2 接續、最終 `["A","B","C","D","E"]` 全部完成、完成後 detect=clean。**證明兩個複合規範參考實作能扛中斷。**

---

## 兩套煙霧測試（161 斷言全綠，已實機確認）

### `smoke_test.py` — 工具端到端（83 斷言）

覆蓋面（依 `check()` label）：
- **indexer**（5）：掃 funcs/ exit 0、找到 upper.py / c_linter.sh、markdown 格式 exit 0、markdown 含標題。
- **router**（4）：路由 upper 並執行、把輸入轉大寫、`--list` 列出三項、不存在 name 回 exit 1。
- **switch**（5）：lang=c exit 0、c→c_linter、python→py_linter、`--explain` 顯示 value、未知 lang 走 default。
- **sfc intake / list / call**：intake python exit 0、functions.json/index.json 已建立、拒絕空 body（exit 1）+ 可讀錯誤、intake shell exit 0、list 含 shout/wc_words、shout（python in-process）→ `HELLO!`、wc_words（shell subprocess）→ `4`。
- **sfc --metadata 變體**（Gap A/B 修法驗證）：`shout --metadata` 不再被攔截 + 回該函式 metadata、頂層 `--metadata` = one_shot dispatcher、`forge --metadata` = **persistent（與頂層不同）**、`intake --metadata` = stateful_external、未知函式 `--metadata` → exit 1、對不存在函式回 exit 1。
- **sfc forge（Layer 2/3/4）**：forge exit 0、回應 5 行、`cmd=list`、記憶體查表呼叫 shout → `FORGE!`、shell wc_words → `3`、不存在函式回 ok=false、shutdown 回 shutdown=true、**add** 動態加入 rev + 立刻能呼叫、**remove** 移除 shout + list 反映增刪、**persist** 回報已存 + 磁碟 store 含 rev 不含 shout、**Layer 4** shell 超時回 timeout 錯誤封套（含 function 欄位）、unknown_function 也是結構化封套。
- **hub**（4）：`--metadata` exit 0、掃出 4 個 skill、含 upper.py/reverse.py、budget 過小時收斂並標註省略。
- **llm_entry_manager**：metadata = persistent、宣告 singleton 資源、complete 回 text+usage、usage 查詢、超預算 → rate limit exceeded（consume rate 守門）；**`--provider echo` 覆寫 flag 跑通 complete**（backend_from_env CLI 覆寫）。
- **chain**（4）：`--metadata` exit 0、跑管線 upper→reverse 得 `IH`、derive 複合 lifecycle=one_shot、derive 複合 guarantee=none（最弱）。
- **idea（2026-06-08 新增）**：metadata = one_shot、`idea clean` 宣告第九軸 `nondeterministic=true`、`ingest` = stateful_external、`clean` 經 entry manager 全鏈跑通（EchoBackend 回顯）、`clean --direct` 跳過 entry manager 也跑通、`ingest` raw 逐字保留（不經 LLM、無 echo 前綴）、`ingest` 產出 cleaned 與 notes、`ingest` slug 連字號後綴不跨主題污染。
- **Gap G 端到端（2026-06-08 新增）**：`entry_manager --socket` 啟動並建立 socket 檔、**兩個獨立 `idea` process 經長駐 socket entry manager → consume rate 跨呼叫累計**（Gap G 修復實證）。

### `lib_smoke_test.py` — lib 各模組（78 斷言）

覆蓋面：
- **state_dirs**（2）：單檔 JSON 往返、`declared()` 合規範。
- **recovery**（6）：半完成→detect=resume、startup 觸發 on_resume、complete→detect=clean、rollback 模式 detect + 觸發、reset 觸發。**recovery 三模式全覆蓋。**
- **memoize**（1）：第一次 miss、第二次 hit 且不重算。
- **server**（7 + socket 2）：exit 0、ping→pong、list 含 handler、自訂 handler upper(hi)=HI、未知 cmd→ok=false、shutdown 回 shutdown=true、ready 訊號進 stderr；**`serve_socket` 跨獨立連線共用 server 狀態（累計 3→7）**、**收 shutdown 後清掉 socket 檔**（Gap G 底層）。
- **singleton**（RateMeter 4 + queue 4 + drain 2）：累計、remaining、would_exceed、尚未超限；queue cancel 中間項、pending 扣掉取消、dequeue 跳過取消項、dequeue 下一個；drain 因超限提前停（處理 3 個）、已超限。
- **trace**（7）：只有一個 root、root=outer、inner 掛在 outer 下、兩段都 complete、render 縮排呈現巢狀、**容忍 incomplete span（缺 end）**、render 標記 incomplete。
- **call**（4）：InProcess 反轉、Subprocess 轉大寫、**Http round-trip（真實 HTTP）**、from_spec 建 Subprocess。
- **llm_call**（3 + 真 backend 8）：預設 EchoBackend、bind 疊 system 進 prompt、bind 疊 postprocess 後綴；**真 backend round-trip（2026-06-08）**——`OpenAIBackend` 解析 `choices[].message.content` / payload 帶 model+messages / 帶 Bearer 授權頭、`AnthropicBackend` 串接 text block / payload 帶 max_tokens / 帶 x-api-key 與 anthropic-version、`backend_from_env` 未設定回 EchoBackend / `openai` → OpenAIBackend。（用攔截式假 HTTP 驗 payload 與 header，不打真網路。）
- **compose**（馴化組合子全覆蓋）：pipe、fanout、fanout_reduce 取最長、route 走 num/txt 分支、with_fallback 主拋例外走備援、decompose 拆-處理-合、retry_until_valid 抽到合格才回 / 用盡拋 ValidationError、vote 自一致取多數=4、best_of 取最高分、guard 驗證失敗走 repair。
- **interact**（6）：run 收斂到 until、max_rounds 安全閥生效、actor_critic 修到驗收 / 第 3 輪收斂 / 永不驗收則用盡輪數、debate judge 取多數=A。
- **compose_meta**（軸推導全覆蓋）：mpipe 行為正確、guarantee=最弱(none)、state=聯集、state_dirs=聯集、列出 persistent 相依；fanout 共用 dir→guarantee 退化 none + 帶 warning、不同 dir→guarantee=idempotent；mretry 拒絕包 guarantee=none（前置契約）、包 idempotent OK。

**特別覆蓋面**（README 強調）：真實 HTTP round-trip、所有馴化組合子、actor-critic 交互、軸推導、recovery 三模式、incomplete span——已逐一在 label 中確認。

---

## 範例函式（funcs/）

供 indexer / router / switch / chain demo 使用，每個都遵守 `--metadata` 契約。

- **`upper.py`** — 把 stdin（或 `--input` 檔）轉大寫。Python，自含 `sys.path` 以保持獨立可執行；`register(lifecycle="one_shot", state="stateless")` + `intercept()` 放在 `main()`。router demo 的目標、chain demo 第一段。
- **`reverse.py`** — 把 stdin 反轉（去尾端換行）。Python，同手法。chain demo 第二段（`echo hi` → upper → `HI` → reverse → `IH`）。
- **`c_linter.sh`** — switch demo 用的假 C linter。讀 stdin 回報「以 C 規則檢查」；**手寫 `--metadata` JSON**（shell 函式）：`{"lifecycle":"one_shot","state":"stateless"}`。
- **`py_linter.sh`** — 假 Python linter，同結構。

---

## SFC store（store/）

SFC Layer 0 的預設 store，**已 seed 兩個範例函式**：

`store/functions.json`：
- **`shout`**（kind=python）：`body = "return stdin.strip().upper() + '!'"`，description「把輸入轉大寫並加驚嘆號」，metadata one_shot/stateless。`echo "hello there" | sfc shout` → `HELLO THERE!`。
- **`wc_words`**（kind=shell）：`body = "wc -w | tr -d ' '"`，description「數輸入有幾個字」，metadata one_shot/stateless。`echo "a b c d" | sfc wc_words` → `4`。

`store/index.json`：恆等映射 `{"index": {"shout": "shout", "wc_words": "wc_words"}}`（spike 階段 name→name，結構上預留升級空間）。
