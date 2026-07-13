# Round 2, Phase 2: 主持人收斂（Synthesis）

> 這份不是第五位專家的發言，是主持人把 Round 2 四份提案**收斂成可開工的 V0 規格草案**，並把懸而未決的點抽出來做裁決建議。

## 0. 一句話結論

**Round 2 收斂成功。** 四位專家不約而同把各自的訴求**全部掛到 SSA 提的同一個 Asset JSON（ATP v0）上**——沒有人再要求獨立子系統。V0 = 讓一個 asset JSON 從 `idea.py` 流到 `sfc.py`，途中被**裁剪（AIRE）/ 定位（SSE）/ 蓋最弱證書章（SGA）/ 全程一個環境變數追蹤（SSA）**。零新框架、零 sqlite、零 bubblewrap、零准入引擎。

## 1. 已凍結的共識（可直接開工，無人反對）

| # | 共識項 | 由誰提、誰背書 |
|---|--------|---------------|
| C1 | **V0 任務 = 在錨點插入一個函式**，成功用確定性驗證（插入後 `ast.parse` 過 + 目標節點存在 + 簽名符合） | SSE 提，AIRE/SSA/SGA 全同意 |
| C2 | **單一資料結構 = ATP v0 asset（append-only envelope）**，逐站追加欄位不改寫上游 | SSA 提，四人把訴求全掛上去 |
| C3 | **隔離 = `subprocess(env=受控白名單)` 軟隔離**，V0 不上 bubblewrap/namespace | SSE 提，SSA 白嫖、SGA 有條件接受 |
| C4 | **Trace = 一個 `AI_CORE_TRACE_ID` 環境變數 + asset 內 append-only `trace[]`**，落盤先寫 NDJSON 不上 sqlite | SSE 底線，SSA 接受並釘語意 |
| C5 | **裁剪 = `skeletonize()` 純 `ast`**（保簽名/錨點鄰域、刪實作），結果是 ephemeral 資產 | AIRE 提，SSE/SSA 有條件同意 |
| C6 | **證書 = asset 上的 `certificate` 欄位（寄生第九軸 dict），標註不攔截，V0 全 `uncertified`** | SGA 降規格、SSE/SSA 同意 |
| C7 | **裁剪安全護欄 fail-closed**：命中敏感名/裝飾器強制保留，裁掉安全節點→該 trace 判失敗（安全凌駕語法） | SGA 提，AIRE 正面接為硬約束 |
| C8 | **降級 tier=3 切 Ollama 是 V0 必做**，不可砍（專案初心） | SSE 堅持，AIRE 用作 benchmark 變因 |
| C9 | **延後到 v0.x/v1**：sqlite、bwrap、socket daemon/併發、hub 多消費者編排、證書 `approved` 簽發、必要性閘 | 四人一致劃線 |

## 2. 統一後的 V0 Asset Schema（融合三人欄位）

把 SSA 的 envelope + AIRE 的 TrimTrace 度量 + SGA 的 certificate 三者合一，去重後的單一結構：

```jsonc
{
  "asset_id": "a-7f3c",
  "trace_id": "t-9b21",                 // C4：與環境變數 AI_CORE_TRACE_ID 同值
  "kind": "code_snippet",               // V0 唯一值
  "schema_version": 0,
  "lifespan": "ephemeral",              // AIRE：裁剪產物用完即焚，不進耐久層

  "origin":  {"producer":"idea.py","model":"claude-sonnet","prompt_version":"...","ts":"..."},
  "payload": {"lang":"python","body":"...","ast_valid":true},

  "pruning": {                          // = AIRE 的 compression 區
    "applied":true, "method":"ast_skeleton",
    "tokens_before":1840, "tokens_after":210, "ratio":0.114,
    "nodes_kept":7, "nodes_dropped":23
  },
  "target":  {"file":"routes.py","anchor":"# AI_CORE:INSERT_HERE","line":42,"resolved_by":"line_assistant"},

  "certificate": {                      // SGA：寄生第九軸語意，標註不攔截
    "v":0, "model":"ollama:qwen2.5-coder:7b", "action":"code_edit",
    "status":"uncertified",             // V0 ∈ {uncertified, syntax_ok}（見裁決 Q3）
    "sandbox":"subprocess_env_stripped",// 由 execute_in_isolation 權威產出
    "test_set":null, "stability":null, "issued_at":"...", "trace_id":"t-9b21"
  },

  "validity": {"is_valid_python":true,"anchor_filled":true,"syntax_error":null},  // AIRE 最簡回饋
  "degradation": {"tier":3,"provider":"ollama","fallback_triggered":true},        // C8 + benchmark 變因
  "safety": {"protected_nodes_preserved":true,"violations":[]},                   // C7 護欄結果

  "trace": [                            // append-only，每站一筆
    {"stage":"produce","tool":"idea.py","ok":true},
    {"stage":"prune","tool":"skeletonize","ok":true},
    {"stage":"locate","tool":"line_assistant","ok":true},
    {"stage":"consume","tool":"sfc.py","ok":false,"reason":"syntax_error"}
  ]
}
```

## 3. V0 要新建/改的檔案（最小集）

| 檔案 | 職責 | 出處 | 行數量級 |
|------|------|------|---------|
| `try_implement/lib/isolation.py` | `build_isolated_env` / `run_isolated` / `execute_in_isolation` | SSE+SGA | ~40 |
| `try_implement/lib/trace.py` | `current_trace_id` / `child_trace_env` + NDJSON append | SSE+SSA | ~30 |
| `try_implement/lib/skeleton.py` | `skeletonize(source, anchor, protected)` + `prune_safety_check` | AIRE+SGA | ~80 |
| `try_implement/lib/asset.py` | ATP v0 asset 建構/追加/序列化 + `cert_status` | SSA+SGA | ~50 |
| `try_implement/tools/line_assistant.py` | 純 Python 錨點→行號定位（不沾第九軸） | SSE+AIRE | ~40 |
| demo: `try_implement/demos/v0_pipeline.py` | 端到端串：idea→hub→sfc，跑通插入函式 + retry + 降級 | 全員 | ~100 |

全部純標準庫（`ast`/`json`/`subprocess`/`uuid`/`os`），不破壞 `dependencies=[]`。

## 4. 待裁決三題（SSA 結尾提問）+ 主持人建議

**Q1（AIRE 答）`pruning.strategy` 列舉值要哪些？**
→ 建議 V0 只凍結 **兩個**：`"raw"`（不裁，benchmark 對照組）與 `"ast_skeleton"`（C5 的 skeletonize）。其餘（`docstring_strip`、`dep_track_deep`）留 v0.x。理由：AIRE 的 pivot 表只需要 raw vs ast_skeleton 兩列就能證明架構支撐。

**Q2（SSE 答）找不到 anchor 時怎麼辦？**
→ 建議 **hub 攔下，不往下走**：`target.line=null` 時 hub 直接判 `trace[+locate].ok=false, reason="anchor_not_found"`，不送進笨模型（省一次 LLM 呼叫，且符合 SGA「拒絕為預設」——定位不到就不執行）。SSE 原傾向讓 sfc 報錯，但 hub 早攔更省成本，建議採此。

**Q3（SGA 答）`syntax_ok` 之外還要別的 status 值嗎？**
→ 建議 V0 凍結 **三個** status：`uncertified`（剛產出）、`syntax_ok`（過 ast.parse，准寫檔）、`rejected`（護欄 fail-closed 或 retry 耗盡）。`approved` 留 v1。`rejected` 是必要的——SGA 的撤照規則需要一個明確的終態，不能讓失敗 asset 停在 `uncertified` 假裝還能救。

> 三題的建議裁決彼此無衝突，若四位專家無異議，schema 即可凍結、V0 開工。

## 5. 下一步（建議）

1. **Round 3（可選）**：只為確認上面三題裁決，一輪即可收斂，不必再全員長篇。
2. **直接開工 V0**：照 §3 檔案清單，先寫 `lib/skeleton.py` + `lib/isolation.py`（兩個最獨立、可單測），再串 `demos/v0_pipeline.py`。
3. **更新 `roadmap.md §6` 與 `DECISIONS.md`**：把本輪凍結的 ATP v0 schema 與三題裁決登記為 v0 的正式切片定義。

---
*產出方式：四份 Round 2 發言由內部並行 subagent 獨立扮演四位專家產生（gemini cli 因 Google 停用免費 tier 不可用）；本收斂由主持人（Claude）撰寫。*
