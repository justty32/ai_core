# impl 統一設計 — 索引（設施面金字塔）

> 狀態：進行中。描述面九軸已全部定案（見各 `axis_N_*.md`）；本階段把散落各軸的**設施面定位**收攏、
> 統一重做設計。**仍是筆記層討論、逐步確認、不寫碼**（沿用描述面階段的鐵則）。

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
| 5 resources | rate-meter：consume-rate 計量 / 限流 / 配額 | `RateMeter` | 相對獨立計量 |
| 6 interruptible | signal(SIGTERM graceful)、checkpoint/resume、reset/rollback | — | **與軸 7 重疊** |
| 7 guarantee | 冪等/交易機器：journal、commit/rollback、dedup key | — | **與軸 6 共用** |
| 8 dry_run | `--dry-run` flag 約定、`is_dry_run()`、預覽輸出導向 | — | 消費軸 1 + 關聯軸 7 |
| 9 nondeterministic | 馴化框架(retry/vote/guard) + 證書簽發/撤照 + 串 entry manager | 馴化 docs | **最大、最上層** |
| 〔膠水〕 | `intercept(argc,argv,meta)`：命中 `--metadata` 序列化 JSON+exit | `_core.py intercept()` | 跨切面，消費 defs、輸出走軸 1 stdout |

## 依賴金字塔

```
L3 願景頂   軸 9 馴化框架（retry/vote/guard + 證書 + 串 entry manager）
L2 行為層   軸 6+7 transaction/journal/checkpoint ｜ 軸 8 dry-run ｜ 軸 5 rate-meter
L1 特化層   軸 2 serve（tcp/pipe）｜ 軸 4 state store（檔案）｜ 膠水 intercept 序列化（stdout）
L0 地基  ──────────────  軸 1 統一 I/O（檔案 / pipe / shm / tcp）  ──────────────
```

**三個結構洞見：**
1. **軸 1 是唯一地基**——軸 2、軸 4、膠水序列化全是它的特化或消費者。先定軸 1 I/O 介面，上面三層才有地基。
2. **軸 6 與軸 7 合併**成一套 transaction/journal/checkpoint helper（兩軸筆記都各自寫了「高度重疊、歸同一設施」）。
3. **軸 5、軸 9 相對獨立但朝上串**——rate-meter 是計量單元；馴化框架坐頂，把下面全部當零件用。

## impl 設計順序

```
① 軸 1 D-IO（地基介面）＋ 軸 4 D-API（一起拍，因軸4⊂軸1）   → impl_1_io.md
② 軸 2 serve（建在①的 tcp/pipe）＋ 膠水 intercept 序列化（建在①的 stdout）
③ 軸 6+7 合併 transaction helper
④ 軸 8 dry-run plumbing、軸 5 rate-meter
⑤ 軸 9 馴化框架（坐頂，串前面全部 + entry manager）
```

全部設施面拍板後，才動手寫 `ac_helper.hpp` + `main.cpp`。
