# impl 統一設計 — 索引（設施面金字塔）

> ⚠️ 歷史／設計討論記錄（2026-06-28 收斂）。**事實基準（設施碼）在 `../impl/*.hpp`**；本檔是設施面的設計脈絡與待辦，非現況查詢處。描述面九軸事實基準見 `../defs/axes.hpp`。

> 狀態：進行中。描述面九軸已全部定案（見各 `axis_N_*.md`）；本階段把散落各軸的**設施面定位**收攏、
> 統一重做設計。**仍是筆記層討論、逐步確認、不寫碼**（沿用描述面階段的鐵則）。
>
> ⚠️ 重照修正（2026-06-27）：**「別管 hub」與 impl 正交**——它砍的是 defs 欄位（給 hub 看什麼），
> impl 設施服務的是**作者**（幫他寫合規程式），不受 hub 消費題影響。故金字塔在 defs 重新思考後**基本完好**，
> 只做：rate-meter 輸入去掉 cpu（軸 5 砍 cpu，且 cpu 本非 consume-rate 輸入）、新增 shell-out helper（見下）。

## 為什麼有這份文件

各軸筆記末尾都有「def/impl 定位」，把設施面**只記了定位、推遲設計**。收攏後發現：九軸設施**不是平行的九套**，
而是一座**全部壓在軸 1 上、逐層收斂的金字塔**。先看全局再逐塊設計，才不會各做各的、地基不一致。

## 九軸設施面收攏表

| 軸 | 設施內容 | Python 線對應 | 性質 |
|---|---|---|---|
| **1 entries** | 跨傳輸統一讀寫：檔案/dir、pipe、shm、tcp/http | `lib/call.Http`（urllib） | **地基** |
| 2 lifecycle | serve helper：one-shot 本體 → 長駐 server | `lib/server.serve_socket` | 軸 1 tcp/pipe 特化 |
| 3 state | **無獨立設施**（委派軸 4） | — | 設施面外包軸 4 |
| 4 state_dirs | 狀態託管：config/cache/state/data 路徑+建目錄+JSON+語意 | — | 軸 1 **檔案傳輸特化** |
| 5 resources | rate-meter：consume-rate 計量 / 限流 / 配額（輸入 memory/gpu/time/network.traffic，**不含 cpu**） | `RateMeter` | 相對獨立計量 |
| 6 interruptible | signal(SIGTERM graceful)、checkpoint/resume、reset/rollback | — | **與軸 7 重疊** |
| 7 guarantee | 冪等/交易機器：journal、commit/rollback、dedup key | — | **與軸 6 共用** |
| 8 dry_run | `--dry-run` flag 約定、`is_dry_run()`、預覽輸出導向 | — | 消費軸 1 + 關聯軸 7 |
| 9 nondeterministic | 馴化框架(retry/vote/guard) + 證書簽發/撤照 + 串 entry manager | 馴化 docs | **最大、最上層** |
| 〔膠水〕 | `intercept(argc,argv,meta)`：命中 `--metadata` 序列化 JSON+exit | `_core.py intercept()` | 跨切面，消費 defs、輸出走軸 1 stdout |
| 〔shell-out〕 | wrapper helper：spawn 子程序＋接 stdin/stdout/stderr＋wait/exit code | `subprocess` | **軸 1 pipe 特化**；brownfield=wrapper 的本職 |

## 依賴金字塔

```
L3 願景頂   軸 9 馴化框架（retry/vote/guard + 證書 + 串 entry manager）
L2 行為層   軸 6+7 transaction/journal/checkpoint ｜ 軸 8 dry-run ｜ 軸 5 rate-meter
L1 特化層   軸 2 serve（tcp/pipe）｜ 軸 4 state store（檔案）｜ shell-out helper（pipe·wrapper）｜ 膠水 intercept 序列化（stdout）
L0 地基  ──────────────  軸 1 統一 I/O（檔案 / pipe / shm / tcp / subprocess）  ──────────────
```

**三個結構洞見：**
1. **軸 1 是唯一地基**——軸 2、軸 4、shell-out helper、膠水序列化全是它的特化或消費者。先定軸 1 I/O 介面，上面才有地基。
2. **軸 6 與軸 7 合併**成一套 transaction/journal/checkpoint helper（兩軸筆記都各自寫了「高度重疊、歸同一設施」）。
3. **軸 5、軸 9 相對獨立但朝上串**——rate-meter 是計量單元；馴化框架坐頂，把下面全部當零件用。

## impl 設計順序

```
① 軸 1 D-IO（地基介面）＋ 軸 4 D-API（一起拍，因軸4⊂軸1）   → impl_1_io.md
② 軸 2 serve（建在①的 tcp/pipe）＋ shell-out helper（建在①的 pipe/subprocess）＋ 膠水 intercept 序列化（建在①的 stdout）
③ 軸 6+7 合併 transaction helper
④ 軸 8 dry-run plumbing、軸 5 rate-meter
⑤ 軸 9 馴化框架（坐頂，串前面全部 + entry manager）
```

全部設施面拍板後，才動手寫 `ac_helper.hpp` + `main.cpp`。

---

## 落地進度（2026-06-27 — batch-greenfield 切片完成，自然收）

依「目標問題＝停止鍵 / 無消費者就延後」，已落地一個**完整可用的 batch-greenfield 垂直切片**，
餘下全部卡在「現在沒有消費者」→ 停。

| 設施 | 狀態 | 落地處 |
|---|---|---|
| 軸 1 I/O：`read_all`/`write_all`/`write_atomic` | ✅ | `impl/io.hpp`（D-IO Round 2：位址字串 + batch） |
| 軸 1 串流 I/O：`Reader`/`Writer`（flow=streaming） | ✅ | `impl/io.hpp`（Round 3，2026-06-28；範圍 std+檔案） |
| 膠水 intercept + `Meta→--metadata JSON` 序列化 | ✅ | `impl/intercept.hpp`、`impl/meta_json.hpp` |
| 軸 4 StateStore（已原子化） | ✅ | `impl/state.hpp`（D-API Round 2） |
| 軸 1 B 接線解析 + 軸 8 `is_dry_run` | ✅ | `impl/cli.hpp` |
| 軸 6+7 transaction（`write_atomic`） | ✅ | `impl/io.hpp` + `impl/state.hpp`（`impl_transaction.md`） |
| **軸 2 serve、shell-out helper** | ⏸ 延後 | stream 群**重機器**（socket/serve/fan-out/SIGPIPE 背壓）——待 serve / LLM 串流逼出；基本 Reader/Writer 已先落地（上一列） |
| **軸 5 rate-meter** | ⏸ 延後 | 無消費者——C++ 線尚無 LLM 呼叫路徑可計量（真消費者是軸 9） |
| 軸 8 預覽輸出導向 | ⏸ 延後 | 小；`is_dry_run` 已足供 app 自行分流 |
| **軸 9 馴化框架** | ⏸ 延後 | 坐頂、最大——需 LLM entry-manager + 真 API（Python 線元件 1/2，C++ 線未有） |

**已成形的契約**：一支依託 ac_helper 寫的程式現在能——宣告九軸 metadata、回應 `--metadata`（吐合法 JSON）、
原子託管狀態（StateStore）、接線解析 I/O、乾跑判斷。g++ 與 CMake 兩條 build 鏈皆綠、零警告。

**再往下的前提**：餘下三塊各需一個**新的目標問題**逼出形狀（serve 的常駐需求 / C++ 側 LLM 呼叫路徑）。
不該在無消費者時憑空蓋——等真需求來。
