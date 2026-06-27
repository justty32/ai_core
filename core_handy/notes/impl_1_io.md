# 軸 1 impl：統一 I/O 設施（地基）

> 狀態：✅ D-IO 拍板（Round 2）：位址字串 + batch 先（`read_all/write_all`），stream 群延後。impl 金字塔的地基（見 `impl_overview.md`）。
> 描述面已定案（`axis_1_entries.md` **Round 13**：`Entry{direction, content, extra}`，已砍 writable）；本檔設計**設施面**。
> **筆記層、不寫碼。**
>
> ⚠️ 重照修正（2026-06-27）：本檔 Round 1 是對著**舊軸 1 Round 11（含 writable）**寫的，已更新欄位引用。
> 「別管 hub」砍的是 defs 欄位、不砍 impl 設施（設施服務作者、與 hub 正交，見 `impl_overview.md`）。

## 設施的兩半（收攏時看清）

| 半 | 職責 | 輸入 → 輸出 |
|---|---|---|
| **A. 讀寫核心** | 真的去讀/寫一個通道 | 傳輸身分 → bytes/text |
| **B. 接線解析** | 把通道名綁到具體傳輸（terminal_binding 的新家） | 通道名 + CLI args → 傳輸身分 |

描述面 `Entry{direction, content}` 傳輸無關、給 hub/呼叫方看；傳輸身分（路徑/`-`/endpoint/名稱）**不在 defs**，
是設施層輸入——brownfield=wrapper 裁決後更明確：傳輸身分**純屬 impl**，defs 永不承載（見 `00_index.md` 全域立場）。

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
| 11 | **subprocess（shell-out）** | spawn 子程序、接其 stdin/stdout/stderr＋wait/exit code（**wrapper 的本職**） |

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

### subprocess（#11）的歸群與定位（2026-06-27 brownfield 裁決後新增）

subprocess 不是單一原始通道，而是**組合多個 pipe 通道 + 程序管理**：spawn 子程序、把它的
stdin/stdout/stderr 各接成一條 pipe（stream 群）、wait 並收 exit code。歸 **stream 群**（一次性消費、
需握住程序）。它建在軸 1 pipe 之上，**是 pipe 的特化/組合**——與軸 2 serve 同性質（serve 也是 pipe/socket
的特化）。

因為 brownfield=wrapper 是一等公民路線（既有 CLI 一律經 wrapper 進系統），**wrapper 的本職就是 subprocess
管理**——這使「shell-out helper」成為軸 1 I/O 的正式消費者，列入金字塔 L1（見 `impl_overview.md`）。

---

## Round 2（✅ D-IO 拍板，2026-06-27）

用「目標問題＝停止鍵」照：第一目標問題（程式碼輔助助手）的工具全是 **batch I/O**（讀 stdin/檔案、
寫 stdout/檔案）；socket/streaming 是 server/LLM 那群（少數），**現在零消費者**。據此兩個決定：

### 決定一：傳輸身分 = 位址字串（URI-ish）

選字串而非結構化 variant：KISS、單參數、shell 天生友善、scheme 可擴充，與專案（軸值皆 string、
shell 一等公民、least-dependency）一致。

- scheme 缺省 ＝ 檔案路徑；`-` ＝ std（in/out 看用在 read 還是 write）。
- `tcp://host:port`、`unix:/sock`、`shm:name` 等 scheme **保留、延後**（stream 群才需要）。

### 決定二：v0 範圍 = batch 先（方案 A），stream 群延後

v0 只做兩個自由函式，涵蓋 std + 檔案（batch 群）：

```cpp
// 概念草圖（筆記層，先不寫進 hpp）
namespace ac::io {
  std::string read_all(const std::string& addr);                          // "-"=stdin / "path"=檔案
  void        write_all(const std::string& addr, std::string_view data);  // "-"=stdout / "path"=檔案
}
```

**延後（等真消費者逼出形狀）**：
- **Channel 物件 + stream 群**（std 串流/pipe/socket/tcp/http）：握連線、`read_chunk`、背壓、SIGPIPE、
  fan-out——這些重機器正是**軸 2 serve / LLM 串流**才逼得出來的，讓那些軸去逼，不憑空猜。
- **subprocess(#11) / shell-out helper**：跟 stream 群一起延後（本就是 L1 特化、非核心通道）。
- **`shm:` / `tcp://` / `unix:` scheme**：同上。

**A 不是死路**：屆時自由函式 `read_all/write_all` 原封不動變成 Channel 的薄糖（(c) 綜合案），
呼叫點零改動。

### 連帶定位

- **軸 4 StateStore ⊂ 軸 1**：建在 `read_all/write_all` 的**檔案分支**上（限定 config/cache/state/data
  4 目錄 + JSON 語意）。待軸 4 D-API 一起拍。
- **膠水 intercept 序列化已解鎖**：`Meta → --metadata JSON` 的輸出就是 `write_all("-", json)`——
  不需 stream 群，現在就能做（見 `impl_overview.md` 設計順序②，膠水那半可先行）。
- **B 接線解析**（entry 名 + CLI args → addr，如 `--input path`）：另一個小 resolver，稍後接。

---

## 待續（下一輪要拍的）
- 軸 4 D-API：StateStore 介面（建在 `read_all/write_all` 檔案分支）。
- 膠水：`Meta → --metadata JSON` 序列化形狀 + `intercept(argc,argv,meta)` 進入點（用 `write_all("-")`）。
- B 接線解析（terminal_binding 新家）：`--input <path>` → 位址字串。
- stream 群 Channel 物件——延後到軸 2 serve / LLM 串流逼出。
