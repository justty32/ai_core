## 4. 資料結構與介面草案

### 4.1 ATP asset schema 的固化增量（在 round_2_synthesis.md 已凍 schema 上加，不改寫上游）

```jsonc
{
  // ... 既有 ATP v0 欄位（asset_id / trace_id / origin / payload / pruning / target / validity）不動 ...

  "certificate": {
    "v": 1,
    "model": "ollama:qwen2.5-coder:7b",
    "action": "code_edit",
    "status": "uncertified",          // ∈ {uncertified, syntax_ok, rejected, approved(v1)}

    // ── 接地憲法（火花1 / [[2601.05280]]）：一級欄位，標明此留白靠什麼接地 ──
    "grounding_class": "formal_executable",  // ∈ {formal_executable, learned, static_proxy}
    "verifier_ref": "ast.parse + signature + anchor_exists + isolation_run",
    "alpha_source": "replay_against_ast_verified_traces",  // αₜ 的具體來源（外生於固化迴圈）

    // ── 固化品質軸（火花10/11/13 + MDL 角度 e）──
    "mdl_bits": 210,                  // 資產描述長度（位元）；優選＝命中率÷mdl_bits（CompressARC）
    "hit_rate": 0.83,                 // matcher 在歷史 trace 的命中比例
    "coverage": 0.91,                 // 規則命中比例（低＝藏起的查表，測謊器）
    "holdout": 0.88,                  // 封存 trace stability（唯一裁判，一票否決脆化）
    "necessity": "irreducible",       // ∈ {separable→撤照候選, irreducible→誠實留 LLM}

    // ── 可逆 / 溯源（前案 K4 / SGA 三重籠子）──
    "forged_from": ["t-9b21", "t-9c07"],   // 由哪些 trace 煉出
    "asset_ref": "a-3d1f",                 // 指回三明治骨架，撤照無損可逆
    "revocations": 0,                      // 撤照前科計數（≥1 禁全自動重固化）

    "test_set": "holdout_v0", "stability": 0.97, "issued_at": "...", "trace_id": "t-9b21"
  }
}
```

### 4.2 wake span（trace NDJSON 的固化原料；前案 §6 缺口，v0 就該補、不可後補）

```jsonc
{"stage":"consume","tool":"sfc.py","ok":false,
 "io":{"in":{"anchor_ctx":"...","sig":"def route(req):"},
       "out":"...","digest":"sha256:..."},   // 大 payload 存 digest+旁路檔
 "cert_draft":{"grounding_class":"formal_executable","ast_verified":true}}
```

### 4.3 固化決策函數簽章（sleep 五步 + 入庫 + 淘汰；全部是普通 ai_core 函數，遞迴閉合）

```python
# ── wake（消費端・零新依賴・純標準庫）─────────────────────────
def emit_llm_span(trace_id: str, *, anchor_ctx: dict, io: dict,
                  cert_draft: dict) -> None:
    """每次 LLM 留白命中 append 一筆 span 到 NDJSON。ast_verified 為 αₜ 真值錨。"""

# ── sleep（離線・貴智能・低頻；嵌入/ILASP 等外部相依只准在此）──
def scan(traces: TraceStore) -> list[Cluster]:
    """語意分群找『反覆出現的同類洞』。嵌入模型在此（工廠側），不滲消費端。[[2604.26311]]"""

def mdl_select(cluster: Cluster) -> list[ForgeProposal]:
    """對每簇用 MDL/壓縮挑『最短能解釋該簇』的 matcher/snippet 候選。
       savings = n·c_old − (c_lib + n·c_new)；優選準則＝命中率÷描述長度。
       [[2512.06104]] [[2603.25975]]"""

def ilp_predicate(pos: list[Span], neg: list[Span],
                  pred_lib: PredicateLib) -> Predicate | None:
    """FP 當負例、FN 當正例餵 ILP（ILASP/Popper），求最具判別力謂詞。
       回 None＝找不到乾淨分離（→ necessity=irreducible，誠實留 LLM）。
       [[2606.24245]] [[2507.16405]]"""

def dedup(proposal: ForgeProposal, library: AssetLib) -> ForgeProposal | None:
    """AST 正規化（改名/常數摺疊/交換律）+ 樹編輯距離；落既有等價類回 None。
       只做保守單步正規化，不追完備等價（會指數爆炸）。[[2501.17848]]"""

def forge(target: Anchor, pairs: list[IOPair], *, policy: ForgePolicy) -> Asset:
    """編成 with_fallback(新規則, 原LLM) 三明治（前案 judge round2），產 forged
       asset + 證書草稿；hindsight relabel：過 ast 閘的失敗 trace 也回收成 asset。
       [[2507.14172]]"""

# ── 入庫閘（領地擴張・三重籠子・v0 需蓋章）──────────────────
def ready(c: Candidate, *, n_min=30, theta=0.95, c_min=0.7) -> ReadyVerdict:
    """四道 AND 閘：n≥n_min ∧ stability≥θ ∧ coverage≥c_min ∧ holdout_ok。
       閘掛 coverage/holdout（不掛 stability，後者在 test_set 100% 是零信息量）。"""

def admit(asset: Asset, v: ReadyVerdict, *, stamp: Approval | None) -> AdmitResult:
    """必要性閘(ilp 可分離) + grounding_class 檢核 + 蓋章。
       v0：領地擴張一律需 stamp；override 表無痛擴張可全自動。"""

# ── 經濟淘汰（庫維護・<100+LRU・可逆）──────────────────────
def evict(library: AssetLib, *, cap=100,
          w=(1.0, 0.3, 0.5)) -> list[AssetId]:
    """破產分數 = w₁·hit_rate − w₂·mdl_bits_norm − w₃·lru_staleness。
       超容量/破產 → 撤照＝路由切回 fallback，不刪資產（K4 可逆）。"""
```

### 4.4 固化引擎 = 三段 chain 的編排（比 forge 高一階，承前案 K5）

```
scan(生態) → 候選簇 ∘ prioritize(候選, trace頻率) → 排程 ∘ forge(目標, pairs, policy) → forged 資產
```

> sleep 五步即 `scan→mdl_select→ilp_predicate→dedup→forge`；`prioritize` 用 trace 頻率排序把貴智能呼叫集中在「高頻同類洞」（呼應 §2 成本梯度）。**v0 對「ready() 准入判斷」本身保持 `nondeterministic:true` 不自我固化**——別讓飛輪自己拆掉自己的剎車（前案 §7）。
