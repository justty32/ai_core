# code_02 — 原型工具（try_implement/tools/）

> **來源**：`try_implement/tools/`（indexer / router / switch / sfc / hub / llm_entry_manager / chain / _common）＋ `try_implement/routes.json`、`try_implement/switch.json`
> **狀態**：原型／探索（多為提案，非定案）。**例外**：A1/A2/A3（register 宣告／攔截拆分）與第九軸 `nondeterministic` 已扶正進正式核心 `src/ai_core/_core.py`，本目錄工具已**改接真 `ai_core`**。
> **一行摘要**：路由／函式管理工具的可跑原型——Indexer、Router、Switch、SFC、Hub、LLM Entry Manager、chain，每個都實作跨元件硬契約 `--metadata`。

相關交叉引用：核心 library 見 [code_01_core_library.md](code_01_core_library.md)；被工具 import 的 library 見 [code_03_prototype_lib.md](code_03_prototype_lib.md)；煙霧測試與 demo 見 [code_04_prototype_demos_tests.md](code_04_prototype_demos_tests.md)；軸／metadata 規範見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)；懸案清單見 [note_06_decisions_and_open_questions.md](note_06_decisions_and_open_questions.md)。

整套只用 Python 3.11+ 標準庫（argparse / json / subprocess / pathlib / urllib…），零外部相依，遵守 KISS / Lightweight / No wheel-remake / Least dependency。

---

## 共通慣例（所有工具都遵守）

1. **`--metadata` 跨元件硬契約**：每個工具都實作 `--metadata`，輸出自身的軸 metadata JSON。Function Hub / Indexer 的可行性完全建立在此契約上。
2. **宣告／攔截拆分模型**（已扶正進 `_core.py`）：
   - `ai_core.register(**meta)` — 純宣告頂層 metadata，**零副作用**（不讀 argv、不攔截、不 exit），import 即安全。
   - `ai_core.intercept(argv=None)` — 在 `main()` 顯式呼叫；命中 `--metadata` 則輸出 JSON 並 exit，否則交還控制權。
   - 慣例：`register*` 放在 `main()` 內（而非 module 頂層），讓工具被當 library `import` 時完全無副作用（解原始 Gap F）。
3. 所有工具的 `register()`/`intercept()` 都放在 `main()` 最前面，先於 argparse。

---

## `_common.py` — 共用 sys.path 小工具

把 `import ai_core`（位於 repo `src/ai_core`）與 `from lib import ...`（位於 try_implement 根）變成可用，純標準庫（sys / pathlib），不安裝套件也不改設定檔。

- `ensure_ai_core_importable()` — 把 `<repo>/src` 插入 `sys.path[0]`，使 `import ai_core` 成功。`<repo>` = `parents[2]`。
- `ensure_lib_importable()` — 把 `<repo>/try_implement` 插入 `sys.path[0]`，使 `from lib import ...` 成功。`parents[1]`。
- `repo_root()` — 回 repo 根 Path。

設計理由：spike 不安裝套件，用「動態塞 sys.path」這個最輕量手段（least dependency）。

---

## `indexer.py` — Indexer

**職責**：掃描指定資料夾的可執行檔，逐一呼叫 `<file> --metadata`，彙整成靜態索引（JSON 或 markdown）。對應 thinking_routing 的「Indexer」與 thinking_sfc 的 Indexer 行。

- **軸**：`lifecycle: one_shot`、`state: stateless`（純讀取掃描；輸出檔由呼叫方指定，視為一般輸出）。
- **可執行判定** `_looks_executable()`：副檔名為 `.py`/`.sh`，或具 exec 權限；跳過隱藏檔與 `_` 開頭（如 `_common.py`）。
- **呼叫 metadata** `_invoke_metadata()`：`.py` 用 `sys.executable`、`.sh` 用 `bash`、其餘直接執行，避免 exec 權限或 shebang 問題（KISS）。回傳非零或無輸出即拋例外。
- **`build_index()`**：回 `{scan_dir, count, functions, errors}`；單一檔失敗收進 `errors` 區，不中斷整批掃描。**此函式被 `hub.py` 重用**——這是把 `register()` 移進 `main()`（而非頂層）的直接動機（解 Gap F）。
- **輸出**：`--format json`（預設，indent=2）或 `--format md`（`render_markdown()`，每函式列出路徑/lifecycle/state/完整 metadata，並有「掃描錯誤」區）。
- **CLI**：`--dir`（必填）、`--format {json,md}`、`--output`（寫檔，否則印 stdout）、`--timeout`（每次 `--metadata` 逾時，預設 10s）。

CLI 範例：
```bash
indexer.py --dir funcs --format md          # markdown 索引
indexer.py --dir tools --format json        # 也能索引工具自己
indexer.py --dir funcs --output index.json
indexer.py --metadata                       # {"lifecycle":"one_shot","state":"stateless"}
```

**實作狀態**：可跑、已扶正接真 `ai_core`。**Indexer 升級版**（AI 加 tags/summary/category）未做（需 LLM，超出 spike）。

---

## `router.py` — Router（thinking_sfc Layer 1b）

**職責**：name → 可執行物 的 mapping，讀 JSON 設定檔查表後以 subprocess dispatch 執行（透傳 stdin/stdout/stderr 與 exit code，KISS：router 全透明）。

- **軸**：`lifecycle: one_shot`、`state: stateless`（router 自身無狀態；被路由目標各自宣告 state）。
- **設定來源**：`--routes <json>`，格式 `{"routes": {"<name>": {"path": "...", "type": "exec"}}}`，path 相對於設定檔目錄解析。
- **`resolve_command()`**：把 target 轉成 argv（依副檔名選 interpreter：`.py`→python、`.sh`→bash）。**只支援 `type=exec`**（單檔程式路徑）；SFC store 內的腳本片段由 `sfc.py` 自己執行（職責分離，非缺漏）。
- **trace-aware**：dispatch 包在 `trace.span(f"router:{name}")` 內，並用 `trace.child_env()` 把 trace id / 父 span 經環境變數帶給子 process——若目標也用 `lib/trace`，其 span 接上同一棵調用樹。即 `lib/trace` 已實際接進工具，不只是獨立 library。
- **CLI**：`--routes`（必填）、`--list`（列出所有路由名後結束）、`name`（要路由的目標）、`rest`（REMAINDER，原樣轉交目標）。

CLI 範例：
```bash
echo "hello" | router.py --routes routes.json upper
router.py --routes routes.json --list
router.py --metadata
```

對應設定檔 `routes.json`：
```json
{ "routes": {
    "upper":  {"path": "funcs/upper.py",     "type": "exec"},
    "c_lint": {"path": "funcs/c_linter.sh",  "type": "exec"},
    "py_lint":{"path": "funcs/py_linter.sh", "type": "exec"}
}}
```

**實作狀態**：可跑、已扶正。**Router 升級版**（安全憑證／資源管理）未做。

---

## `switch.py` — Switch（條件式 dispatcher）

**職責**：在 mapping 之外加條件判斷的 router——依「switch 變數值」走規則表分支到不同 target。典型例子：依輸入語言（c / python）路由到不同 linter。對應 thinking_routing 的「Switch」。

- **軸**：`lifecycle: one_shot`、`state: stateless`。
- **條件表達（spike 決策）**：純資料的**規則表（rule list）**，每條規則做「key 的值 == 常數」字串相等比較，命中即路由。**無 DSL、無 `eval`、不造規則引擎**（不重造輪子）。LLM 易生成、人易讀。範圍／正則等複雜條件留待正式規範。
- **switch 變數值來源** `decide_value()`：`source: "arg"`（取自 CLI flag，如 `--lang c`）或 `source: "ext"`（取自 `--input` 檔副檔名）。
- **`pick_target()`**：逐條比對 `equals`，未命中回 `default`（選填）。
- **`resolve_command()`**：與 router 同邏輯（spike 刻意不抽共用模組，見 Gap D）。
- **CLI**：`--config`（必填）、`--lang`（source=arg 的判斷值）、`--input`（source=ext 時取副檔名）、`--explain`（只印會路由到哪個 target，不執行）、`rest`（REMAINDER，用 `--` 分隔轉交）。

CLI 範例：
```bash
echo "int main(){}" | switch.py --config switch.json --lang c
echo "print(1)"     | switch.py --config switch.json --lang python
switch.py --config switch.json --lang rust --explain   # 只看決策（未命中走 default）
switch.py --metadata
```

對應設定檔 `switch.json`：
```json
{ "switch": {
    "on": "lang", "source": "arg",
    "rules": [
      {"equals": "c",      "target": {"path": "funcs/c_linter.sh",  "type": "exec"}},
      {"equals": "python", "target": {"path": "funcs/py_linter.sh", "type": "exec"}}
    ],
    "default": {"path": "funcs/c_linter.sh", "type": "exec"}
}}
```

**實作狀態**：可跑、已扶正。

---

## `sfc.py` — SFC（Small Function Center）

**職責**：把大量 tiny function 集中到一個 git-style subcommand dispatcher，避免檔案爆炸。涵蓋 Layer 0（store）/ 1a（intake）/ 1b（router）/ 2（forge server）/ 3（動態 add/remove/persist）/ 4（shell-kind timeout + 標準錯誤封套）。

> **已改接真 `ai_core`**：`import ai_core as meta`。原本繞過 library 的 `meta_core.py` 原型已**刪除**（見下方說明）。

### metadata 設定（解 Gap A/B/F，已扶正進 `_core.py`）

`_setup_metadata()` 用拆分模型宣告**單一執行檔多種 lifecycle**：
- `meta.register(lifecycle="one_shot", state="stateless")` — 頂層 dispatcher 預設行為。
- `meta.register_subcommand("forge", lifecycle="persistent", state="stateless")` — **forge 是 persistent**（解 Gap B）。
- `meta.register_subcommand("intake", lifecycle="one_shot", state="stateful_external")`。
- `meta.register_subcommand("list", ...)`、`meta.register_subcommand("admin", ...)`（皆 one_shot/stateless）。
- `meta.register_subcommand_resolver(_resolve_tiny_metadata)` — 動態子命令：tiny function 名稱來自 store，metadata 存在 store 不寫死，resolver 去查。
- `main()` 開頭 `meta.intercept(argv)` 統一攔截所有 `--metadata` 變體（含 `<sub> --metadata`、`--store` 前綴）。

**`--metadata` 行為（重點，已驗證）**：
| 指令 | 回傳 |
|---|---|
| `sfc --metadata` | 頂層 dispatcher → `one_shot` |
| `sfc forge --metadata` | `persistent`（與頂層不同 lifecycle，Gap B 修法效果）|
| `sfc intake --metadata` | `stateful_external` |
| `sfc <tinyfn> --metadata`（如 `sfc shout --metadata`）| 走動態 resolver 去 store 查該函式 metadata |
| `sfc <未知> --metadata` | exit 1 |

### Layer 0 — store（純資料存取）

`class Store`：封裝 `functions.json` + `index.json` 的讀寫。
- **格式決策（thinking_sfc 標為待設計）**：`functions.json` 是 **object**（key=函式名，O(1) 查找），`index.json` 是 `{"index": {"<name>": "<store key>"}}`。保留獨立 index.json 作為「可被獨立擴充的查找層」（未來放 tags/summary/category），spike 階段先是恆等映射。全 JSON、純標準庫可解析。
- 函式定義欄位：`name` / `kind`（`python`｜`shell`）/ `body` / `description` / `metadata`（沿用 ai_core 軸）。
- `DEFAULT_STORE_DIR` = `try_implement/store`；可由 `--store <dir>`（須在最前）覆寫。

### tiny function 呼叫介面（thinking_sfc 待設計）

`compile_function(defn, shell_timeout=None)` 把定義編譯成 callable `fn(stdin: str, args: dict) -> str`：
- **`kind="python"`**：body 是函式體原始碼，包成 `def _fn(stdin, args):`，`compile`+`exec` 到**受限 namespace**（只放少量 builtins：len/str/int/float/range/list/dict/sum/sorted/min/max/enumerate/split/print）後**真正 in-process** 執行。空 body / 只有註解 → 明確攔截給可讀錯誤（避免 cryptic IndentationError）。受限 namespace 是 spike 等級薄防護，**非沙箱**（Gap E）。
- **`kind="shell"`**：body 是 shell 指令，SFC 內部開 `bash -c` subprocess，stdin 餵進、stdout 收回。`shell_timeout`（Layer 4）超時拋 `ToolTimeout`。**timeout 只對 shell-kind 生效**（python-kind 真 in-process 無乾淨隔離邊界，Gap E）。
- 剩餘 CLI flag 由 `_rest_to_dict()` 依 cli_spec §2.0（Lisp keyword pair → dict）轉成 `args`（`--key value`→dict、`--flag`→True、bare positional→`_positional` list）。

### Layer 1a — intake（`cmd_intake`）

把片段納入 store。支援 `--from-json <file>` 或 `--name/--kind/--body/--description`。**收進前先 `compile_function()` 驗證能編譯**（fail fast，給乾淨錯誤而非 traceback），通過才寫 `functions.json` + `index.json`（恆等映射）。

### Layer 1b — call（`call_function`）

讀檔查表執行單一函式：`store.get(name)` → `compile_function()` → 讀 stdin → 跑 → 寫 stdout（補尾換行）。subcommand-scoped `--metadata` 已在 `main()` 最前由 `meta.intercept()` 處理掉，不會走到這裡。

### Layer 2/3 — forge（`cmd_forge`，persistent server）

啟動時把 store 內所有 tiny function 編譯成 callable 存進記憶體 dict，之後純記憶體查表執行。對外用 **NDJSON 行協議**（每行一個 JSON request/response，stdin/stdout）。整個 serve 迴圈包在 `trace.span("forge.serve")` 內成為調用樹 root，每次 call 是 `trace.span(f"forge.call:{name}")` 的 child（trace 事件走 stderr）。啟動時印 `[forge] ready` 訊號到 stderr。

協議：
- **call**：`{"call":"<name>","stdin":"...","args":{...}}` → `{"ok":true,"stdout":"..."}`。
- **Layer 3 管理 API**：`{"cmd":"list"}` / `{"cmd":"add","defn":{...}}`（執行期動態編譯加入）/ `{"cmd":"remove","name":"..."}` / `{"cmd":"persist"}`（把記憶體定義寫回 Layer 0 store，與 intake 對稱）/ `{"cmd":"shutdown"}`。

### Layer 4 — 錯誤封套 + 資源（部分做）

`_err(etype, message, function?)` 產出**標準錯誤封套** `{"ok":false,"error":{"type","message","function"?}}`，取代扁平字串，讓 caller 依 type 分流（如 timeout 可重試、bad_request 不該重試）。type ∈ `bad_json` / `bad_request` / `unknown_function` / `compile_error` / `runtime_error` / `timeout`。

`cmd_admin`（`sfc admin`）已退化為**指路牌**：管理操作請對 running 的 forge server 發 NDJSON。Layer 4 的資源管理（記憶體/時間上限）對 python-kind 仍是 **TODO**（Gap E：stdlib 做不到乾淨隔離）。

### CLI 進入點

git-style：保留字 `{intake, list, forge, admin}`，其餘字串視為 tiny function 名稱（Layer 1b）。

CLI 範例：
```bash
sfc list
echo "hello there" | sfc shout      # python in-process → "HELLO THERE!"
echo "a b c d"     | sfc wc_words    # shell subprocess → 4
sfc shout --metadata                 # subcommand-scoped metadata
sfc --metadata                       # SFC 自身（one_shot）
sfc intake --name rev --kind python --body "return stdin.strip()[::-1]"
# forge persistent server（NDJSON）
printf '%s\n%s\n%s\n' '{"cmd":"list"}' '{"call":"shout","stdin":"hi"}' '{"cmd":"shutdown"}' | sfc forge
# Layer 3 動態管理
printf '%s\n%s\n%s\n' \
  '{"cmd":"add","defn":{"name":"rev","kind":"python","body":"return stdin.strip()[::-1]"}}' \
  '{"call":"rev","stdin":"abc"}' '{"cmd":"persist"}' '{"cmd":"shutdown"}' | sfc forge
```

**實作狀態**：Layer 0–3 ✅ 已做；Layer 4 部分（shell timeout + 錯誤封套已做，python 資源上限 + retry 策略未做）。

### `meta_core.py` 已刪除（2026-05-26）

原型 `tools/meta_core.py` 曾提供「放寬攔截 + `register_subcommand` + resolver」來繞過原始 `_core.py` 的限制。這些能力已**扶正進 `src/ai_core/_core.py`**（採宣告／攔截拆分），原型功成身退、檔案已刪，`sfc.py` 改接真 library。

---

## `hub.py` — Function Hub（規範未定，本版自定義）

**職責**：把整個函式生態轉成「給 LLM 看的 skill 清單」的透鏡（CLAUDE.md 元件 4）。規範完整定義未定，使用者授權自訂的**最小可用定義**。在 Indexer 之上多做兩件事：(1) **語意化**——把每函式 metadata 轉成一條技能描述；(2) **預算化（核心）**——套 context budget，超過就逐級收斂，避免清單撐爆 LLM context。

- **軸**：`lifecycle: one_shot`、`state: stateless`。
- **重用 Indexer**：`from indexer import build_index`（已可安全 import，因 register 移進 main）。
- **`to_skill()`**：把 index entry 轉成 `{name, summary, lifecycle, side_effects, retriable, path}`。
- **summary 來源優先序**：metadata 的 `description`/`summary` 欄位 → 沒有就 `_synthesize_summary()` 從軸值硬湊（如 one_shot+stateless →「一次性、無副作用」）。⚠️ **八軸沒有語意用途欄位（Gap C）**，所以 hub 只能合成粗略 summary——**這把缺口暴露得很具體：沒有語意描述軸，skill 清單對 LLM 幾乎沒用。**
- **budget 逐級收斂** `render_skills()`：`full`（多行/技能）→ `compact`（一行 `name — summary`）→ `truncated`（截斷 summary 並附「另有 N 項未列出」）。
- **CLI**：`--scan`（必填）、`--format {skills,json,md}`（預設 skills）、`--budget`（字元預算，超過自動收斂）、`--timeout`。

CLI 範例：
```bash
hub.py --scan funcs
hub.py --scan funcs --budget 60       # 過小預算 → 自動收斂並標註省略
hub.py --scan funcs --format json
hub.py --metadata
```

**實作狀態**：最小自定義版可跑；完整定義仍待規範定奪。是 Gap C 的具體放大證據（見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md) 與 [note_06_decisions_and_open_questions.md](note_06_decisions_and_open_questions.md)）。

---

## `llm_entry_manager.py` — LLM Entry Manager（CLAUDE.md 元件 1）

**職責**：把「LLM 是 singleton 資源」落地——本地/遠端模型一次只能處理一個請求，且請求者是多個互不認識的 caller → queue（一次一個）+ consume rate（token/錢/算力）管理。示範三概念疊起來：persistent server（`lib/server`）+ singleton 資源（`lib/singleton` 的 RateMeter）+ 可插拔 LLM backend（`lib/llm_call`，預設 mock）= LLM Entry Manager。

- **軸**：`lifecycle: persistent`、`state: stateless`、`resources: {singleton: true, consume_dimensions: ["token"]}`。（singleton 性質塞進 resources——八軸沒有專門的 singleton 欄位，這是一個觀察。）
- **`build_server()`**：建 `NDJSONServer`，註冊 `complete` / `usage` handler；`SingletonResource("llm", limits={token})` 管 consume rate。`complete` 先用 `meter.would_exceed()` 守門，超預算回 `{"ok":false,"error":"rate limit exceeded",...}`；否則跑 `llm_call.llm_call()`、`meter.record()` 記用量。
- **token 估算** `_estimate_tokens()`：mock 約 4 字元 1 token。真接 API 時換成 backend 的 usage。
- **backend**：預設 `EchoBackend`；真接 API 時換接 `lib/call.Http` 或官方 SDK。
- **CLI**：`--limit-token`（token 預算上限）、`--metadata`。NDJSON 協議：`{"cmd":"complete","prompt":"...","opts":{...}}`、`{"cmd":"usage"}`、內建 `ping`/`list`/`shutdown`（來自 `lib/server`）。

CLI 範例：
```bash
printf '%s\n%s\n%s\n' \
  '{"cmd":"complete","prompt":"hello world"}' \
  '{"cmd":"usage"}' '{"cmd":"shutdown"}' | llm_entry_manager.py --limit-token 50
llm_entry_manager.py --metadata
```

**實作狀態**：可跑原型（mock backend）。

---

## `chain.py` — chain（組合維度的 CLI）

**職責**：把「組合維度」變成可從 shell 用的一等工具（對應組合維度概念，見 [doc_21_composition_dimension.md](doc_21_composition_dimension.md)）。用 JSON 宣告一條 pipeline，把 stdin 依序流過每個 stage（subprocess）= 調用鏈的顯式形式；並能用 `lib/compose_meta` 從各 stage 的 `--metadata` 推導整條鏈的複合 metadata。串起整個 lib 堆疊：`lib/call`（跨邊界呼叫成員）+ `lib/compose`（pipe 串接）+ `lib/trace`（調用鏈追蹤）+ `lib/compose_meta`（推導複合 metadata）。

- **軸**：`lifecycle: one_shot`、`state: stateless`。
- **spec 格式**：`{"name":..., "stages":[{"path":"funcs/upper.py","type":"exec"}, {"path":"funcs/reverse.py","type":"exec","args":["--flag"]}]}`，path 相對於 spec 檔目錄。
- **`_resolve()`**：把 stage 轉 argv（依副檔名選 interpreter）——這是 router/switch 的 `resolve_command` 的**第三份**幾乎相同實作，Gap D 訊號再強一點（spike 仍各自獨立維持單檔自足）。
- **`run_pipeline()`**：每 stage 包成 `call.Subprocess`，再用 `compose.pipe` 串起，整條包在 `trace.span(f"chain:{name}")` 內。
- **`derive_metadata()`**：對每 stage 呼叫 `--metadata`，用 `compose_meta.derive_pipe()` 算複合 metadata。
- **CLI**：`--spec`（必填）、`--derive`（不執行，改印推導的複合 metadata）、`--metadata`。

CLI 範例：
```bash
echo hi | chain.py --spec chain_demo.json   # upper→reverse → "IH"
chain.py --spec chain_demo.json --derive    # 推導複合 metadata（lifecycle=one_shot、guarantee 取最弱=none）
chain.py --metadata
```

**實作狀態**：可跑原型，已扶正接真 `ai_core` 與 lib 堆疊。

---

## 工具層發現的設計缺口（簡表，詳見 note_06）

- **Gap A/B/F**：✅ 已扶正進 `_core.py`（subcommand-scoped metadata、單檔多 lifecycle、register 無 import 副作用）。
- **Gap C**：metadata 缺語意用途欄位 → hub 的 skill 清單只能硬湊 summary（未決）。
- **Gap D**：router/switch/chain 三份重複的 `resolve_command`（DRY vs 單檔自足張力，未決）。
- **Gap E**：python-kind in-process tiny function 無真正資源/安全隔離（Layer 2 vs Layer 4 張力，未決）。
