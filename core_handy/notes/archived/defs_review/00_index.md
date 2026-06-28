# defs 不足審查 — 索引

對 `core_handy` 九軸**描述面（defs）**定案的逐軸審查：對照權威（`src/ai_core/_core.py`、
`kb-ss/doc_05_axes_metadata.md`、`doc_06_lib_contract.md`、`roadmap.md`），找出濃縮後的不足、
被掏空的描述能力、與自家原則的內在矛盾。

> 立場：這些**不是「定案錯了」**，而是「定案在某些維度押了賭注（多半是 KISS 往下砍欄位），
> 而賭注的前提（hub 不需要 / 程式都是新寫的 / 跨軸靠文件約束）尚未被明確驗證」。
> 多數懸案的解法不是加欄位，而是**先回答「hub 消費什麼、要不要服務 brownfield」**，再決定欄位去留。

## 逐軸檔案

| 軸 | 檔案 | 最尖的一條 |
|---|---|---|
| 1 `entries` | [axis_1_entries.md](axis_1_entries.md) | Round 11 重開了 Round 7 解掉的數值通道；方向/傳輸身分缺失，hub 無法組合 |
| 2 `lifecycle` | [axis_2_lifecycle.md](axis_2_lifecycle.md) | 流動模式(streaming)在軸 1/2 間流浪、無正式家；常駐光譜壓平成一個 bool |
| 3 `state` | [axis_3_state.md](axis_3_state.md) | 「讀外部 vs 寫外部」被摺平 → 只讀工具兩種填法都失真 |
| 4 `state_dirs` | [axis_4_state_dirs.md](axis_4_state_dirs.md) | 重定位為純設施後，hub 失去「副作用落哪、多嚴重」的可見度 |
| 5 `resources` | [axis_5_resources.md](axis_5_resources.md) | 值無量綱、無格式強制，hub 排程建在不可靠字串解析上 |
| 6 `interruptible` | [axis_6_interruptible.md](axis_6_interruptible.md) | 開放 unsigned 碼表混了「有序階梯」與「不可比擴充值」，序語意自相矛盾 |
| 7 `guarantee` | [axis_7_guarantee.md](axis_7_guarantee.md) | 濃縮最佳；唯冪等鍵等重試細節落 extra |
| 8 `dry_run` | [axis_8_dry_run.md](axis_8_dry_run.md) | bool 化假設 lib 全控 flag，放棄 brownfield（git -n / terraform plan）包裝 |
| 9 `nondeterministic` | [axis_9_nondeterministic.md](axis_9_nondeterministic.md) | 靈魂軸的核心（治理證書）被降到最低保真 extra；量值無單位 → 跨工具不可比 |

---

## 跨軸主題（反覆出現的根因，比單軸問題更值得先決）

### 主題 1：關鍵資訊系統性沉入 `extra`
hub 實際要用的東西反覆落在低保真 string 袋：
傳輸身分(軸1)、流動模式/常駐子型(軸1·2)、冪等鍵(軸7)、dry-run flag(軸8)、治理證書(軸9)。
用他們自己的**升級壓力**尺，這些多半「普遍且必須」，**本該升固定/opt 欄位**。
**根因**：濃縮優先把欄位往下砍（KISS），但沒回頭問「hub 消費什麼」來決定哪些該保留結構。
→ 解法不是全部升格，而是先定 hub 的消費清單（見「該先決的一題」）。

### 主題 2：跨軸非法/無意義組合一律未在型別層閉合
`stateless × state_dirs`(3×4)、`one_shot × idle`(2×5)、`stateless × transactional`(3×7)、
`uncertainty × 證書`(9 內)。C++ 線的賣點「讓非法狀態無法被表達」在**單軸內**貫徹得好，
**跨軸全部放棄、退回文件約束**。
→ 要嘛做一層「總 metadata 的 cross-field 驗證」，要嘛在文件**明文宣告「跨軸不保型別保證」**，
別讓賣點在跨軸默默失效。

### 主題 3：「沒填」vs「明確宣告保守值」不可區分
凡用 zero-init bool/unsigned 的軸（1·2·6·8·9），缺席與保守值塌縮成同一狀態。
對**安全/治理攸關**軸（6 interruptible、9 governance）這個區分有意義（「沒想過」該比「確定危險」更被保守對待），
卻被抹平。`optional` 能保留它、裸 bool 不能。
→ 至少在安全/治理軸評估改用 `optional`。

### 主題 4：brownfield 盲點
軸 1 transport、軸 8 dry-run flag 的降級，都假設「程式是用本 lib 新寫的、lib 能統一約定介面」。
但 roadmap 大量是**馴化既有 CLI**（git / rsync / terraform），它們的 flag/介面 lib 管不到。
濃縮對 greenfield 友善、對 brownfield 包裝丟資訊——而 brownfield 正是「shell/app 作為函式」的主場。
→ defs 要不要**正式承認「包裝既有程式」這一類**，並為它保留描述既有介面的欄位？

### 主題 5：「讀 vs 寫」的區分反覆缺席
軸 1 `writable` 把 direction+mutation 摺一起、軸 3 `stateful` 把讀外部+寫外部摺一起——
同一個「只讀 vs 會改」張力在兩軸都被壓掉，且兩軸對「有無副作用」的表達**冗餘且無互相約束**。
→ 與主題 2 相關：軸 1 與軸 3 的權威關係該明定。

---

## 該先決的一題（多數懸案的共同樁）

**hub 到底消費 `--metadata` 的哪幾維、要不要服務 brownfield 包裝？**

- 答「hub 只做粗排程、不自動組合、只服務 greenfield」→ 現狀九軸幾乎夠，主題 1/4 的多數懸案自動消解，
  只需把「放棄了什麼」寫進文件。
- 答「hub 要自動組合 + 馴化既有 CLI」→ 主題 1（傳輸身分/流動模式/dry-run flag）與主題 4 的欄位**確需補回**。

建議下一步：對 `kb-ss/doc_13_arch_hub.md`、`doc_08_composite.md`、`roadmap.md`（馴化既有框架那段）求證
hub 的真實消費維度與 brownfield 比重，再回頭逐軸落定欄位。**先定受眾，再定欄位。**
