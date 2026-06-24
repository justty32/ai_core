# 軸 1 impl：統一 I/O 設施（地基）

> 狀態：探索中（Round 1 窮舉）。impl 金字塔的地基（見 `impl_overview.md`）。
> 描述面已定案（`axis_1_entries.md` Round 11：`Entry{text, writable, extra}`）；本檔設計**設施面**。
> **筆記層、不寫碼。**

## 設施的兩半（收攏時看清）

| 半 | 職責 | 輸入 → 輸出 |
|---|---|---|
| **A. 讀寫核心** | 真的去讀/寫一個通道 | 傳輸身分 → bytes/text |
| **B. 接線解析** | 把通道名綁到具體傳輸（terminal_binding 的新家） | 通道名 + CLI args → 傳輸身分 |

描述面 `Entry{text, writable}` 傳輸無關、給 hub 看；傳輸身分（路徑/`-`/endpoint/名稱）不在 defs，是設施層輸入。

---

## Round 1：窮舉（通道 × 維度）

### 常用入口（通道種類）

| # | 入口 | 一句話 |
|---|---|---|
| 1 | file（input specific） | 指定路徑讀 |
| 2 | file（output specific） | 指定路徑寫 |
| 3 | file（in-place） | 讀＋寫同一檔 |
| 4 | dir | 多檔/掃描（軸 4 託管用） |
| 5 | std streams | stdin/stdout/stderr（pipe 特例、`-` 慣例） |
| 6 | pipe / FIFO | 匿名管／具名管，streaming 點對點 |
| 7 | shared memory | `/dev/shm`、mmap |
| 8 | unix socket | 本機 IPC（軸 2 serve 用） |
| 9 | tcp | 跨機 byte stream |
| 10 | http(s) | 建在 tcp 上的 request/response |

### 分類維度（驅動設施設計的軸）

| 維度 | 值域 | 為何影響設施 |
|---|---|---|
| D1 流動 | batch（一次讀完）／ stream（持續、逐塊） | 決定 `read_all` vs `read_chunk` |
| D2 落硬碟 | ✓／✗ | 真實磁碟 IO（成本、ENOSPC、fsync） |
| D3 可重讀/seek | seekable（可回頭、可重讀）／ 一次性消費（讀過就沒） | 能否 retry/重放 |
| D4 連線生命週期 | open 即用（無握手）／ 需握手·attach 並保持 | 逼出「握住連線」的物件 |
| D5 方向 | R／W／RW | open mode |
| D6 壽命 | 持久（跨 process）／ 隨 process／ 隨連線 | 狀態存活範圍 |
| D7 阻塞·背壓 | 無／ 有（等對端、背壓、SIGPIPE） | 是否需處理阻塞與斷裂 |
| D8 定址 | path／ fd／ name／ host:port·URL | B 接線解析的輸入形狀 |
| D9 多對端 | 點對點／ 可多連線（fan-in/out） | server fan-out（軸 2） |

### 矩陣（通道 × 維度）

| 入口 | D1流動 | D2碟 | D3重讀 | D4連線 | D5向 | D6壽命 | D7阻塞 | D8定址 | D9多端 |
|---|---|---|---|---|---|---|---|---|---|
| file in | batch | ✓ | seek | 無 | R | 持久 | ~無 | path | 點對點 |
| file out | batch | ✓ | seek | 無 | W | 持久 | ~無 | path | 點對點 |
| file in-place | batch | ✓ | seek | 無 | RW | 持久 | ~無 | path | 點對點 |
| dir | 列舉 | ✓ | n/a | 無 | RW | 持久 | ~無 | path | n/a |
| std streams | batch\|stream | ✗ | 一次性 | 無 | R\|W | process | 有 | fd(0/1/2)·`-` | 點對點 |
| pipe/FIFO | stream | ✗ | 一次性 | FIFO 需兩端 | 單向 | process\|FIFO節點持久 | 有(SIGPIPE) | path·fd | 點對點 |
| shm | batch\|隨機 | tmpfs(半) | seek(隨機) | 需 attach/map | RW | 跨process(隨cleanup) | 無(但需同步) | name | 多方共享 |
| unix socket | stream | ✗ | 一次性 | 需握手保持 | RW | 連線 | 有 | path | 可多連線 |
| tcp | stream | ✗ | 一次性 | 需握手保持 | RW | 連線 | 有 | host:port | 可多連線 |
| http | req/resp(可chunk) | ✗ | 一次性 | 連線(可keep-alive) | RW(req→resp) | 連線/請求 | 有 | URL | 請求式 |

### 浮現的兩群（重要！這群分界會驅動介面抉擇）

維度沿同方向聚成兩群：

- **batch 群**（file / dir / shm）：`batch + 落碟/可定址 + seek + 無連線`
  → **自由函式 + 位址字串**就夠：`read(uri)` 一次讀完、`write(uri, data)` 一次寫完。shm 可 mmap 後當 buffer 隨機存取。
- **stream 群**（std / pipe / socket / tcp / http）：`stream + 一次性消費 + 需保持連線/背壓`
  → 逼出**通道物件**：握住 fd/連線、`read_chunk` 逐塊、處理阻塞/SIGPIPE、軸 2 server fan-out。

> 這正好印證先前 (c) 綜合案：**batch 群走自由函式糖、stream 群下沉 Channel 物件，一份實作兩種門面**。
> 但**先別拍**——本輪只窮舉。下一輪再決定介面長相（D-IO）與傳輸身分表示法。

---

## 待續（下一輪要拍的）
- D-IO：讀寫核心介面長相（自由函式 / 通道物件 / 綜合）。
- 傳輸身分表示法：URI/路徑字串 vs 結構化 variant。
- B 接線解析（terminal_binding 新家）：`--input <path>` → 傳輸身分。
- 與軸 4 分層：StateStore 包 io（軸4⊂軸1 檔案傳輸）。
- 維度是否窮盡／要不要增刪通道（待你補）。
