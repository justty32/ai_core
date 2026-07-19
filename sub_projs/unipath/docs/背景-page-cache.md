# 背景知識：Page Cache（頁快取）

> unipath 先驗知識。承 [背景-fuse與vfs.md](背景-fuse與vfs.md) §7 那句「核心對 FUSE 有快取（屬性快取、page cache）」——這篇把那句話拆開講清楚：
> page cache 到底是什麼、read/write 怎麼穿過它、為什麼 unipath 的「執行態非快照」這個招牌主張，恰好卡在這層快取機制的縫隙上。

## 0. 先立柱子：page cache 是什麼

核心把**讀過、寫過的檔案內容**，以「頁」（通常 4KB）為單位快取在 RAM 裡。之後同一頁的 `read`，只要還在快取裡，**直接從記憶體複製給你，完全不碰磁碟**。

- 快取的 key 概念上是 `(inode, 頁號)`——同一個檔案的同一段內容，不管誰開、開幾次，命中的是**同一份**快取頁。
- 這塊記憶體不是「留白浪費」，是核心刻意的：`free -h` 看到的 `buff/cache` 欄位大半就是它。系統記憶體吃緊時，這塊會被優先回收（乾淨頁可以直接丟，反正磁碟上還有一份）。
- 效果：第一次 `cat bigfile` 慢（真的讀磁碟），第二次幾乎瞬間（純記憶體命中）。這是「用記憶體換速度」最日常的體現。

**沒有 page cache 的世界**：每次 `read` 都要下探到磁碟裝置，延遲以毫秒計；**有了它**：命中時延遲以奈秒／微秒計——這正是一般檔案系統「感覺很快」的主因，不是磁碟真的變快了。

## 1. read/write 如何穿過 page cache

```
read(fd, buf, n)
    ▼
VFS：查頁是否已在 page cache
    ├─ 命中 → 直接把頁內容複製進 buf，返回（沒碰磁碟）
    └─ 未命中 → 呼叫底層 fs 的回呼，把該頁從磁碟讀進一塊新的 cache 頁
              → 複製進 buf、返回；那頁**留在 cache 裡**給下次用

write(fd, buf, n)
    ▼
VFS：頁若不在 cache 且不是整頁覆寫 → 先讀入該頁（保留頁裡沒被覆寫的部分）
    ▼
把 buf 內容蓋寫進那塊記憶體頁（純記憶體操作）
    ▼
把該頁標成 **dirty**
    ▼
立刻返回 —— **write() 成功回傳，不代表資料已落盤，只代表已進 RAM**
```

這就是為什麼一般「buffered write」感覺很快：`write()` 本質上只是改一塊記憶體再打個標記，磁碟 I/O 被推遲到之後、由核心背景處理。

## 2. dirty page（髒頁）與 writeback（回寫）

- **dirty page**：被 `write` 改過、但**還沒刷回底層儲存**的快取頁。`/proc/meminfo` 的 `Dirty:` 欄位就是目前系統裡髒頁的總量。
- **writeback**：核心背景執行緒（`kworker`/flusher thread）定期把髒頁刷回磁碟，刷完清掉 dirty 標記，頁變回「乾淨」（可被安全回收）。觸發時機受幾個閾值控制：定期（如 `dirty_writeback_centisecs`，預設約每 5 秒巡一次）、髒頁比例超過 `dirty_background_ratio` 就主動背景刷、超過 `dirty_ratio` 則連呼叫 write 的 process 都會被卡住（同步刷）等到有空間。

**危險的縫隙**：`write()` 回傳成功 ≠ 資料已在磁碟上。如果這時斷電/核心崩潰，**還沒 writeback 的髒頁會整批消失**，檔案回到上一次落盤的狀態——即使程式邏輯上早就「寫完了」。這是 buffered I/O 最著名的坑，也是下一節 `fsync` 存在的理由。

## 3. `fsync` / `fdatasync` / `sync`：強制回寫

| 呼叫 | 保證什麼 | 特性 |
|---|---|---|
| `fsync(fd)` | 該 fd 對應檔案的**所有髒頁 + 中繼資料**（mtime、大小…）都刷到底層裝置，且等硬體確認落盤 | 最完整，也最慢 |
| `fdatasync(fd)` | 只保證**資料本身**落盤；若中繼資料的變動不影響之後讀取正確性（例如 mtime），可以省略那筆更新 | 通常比 fsync 快一點 |
| `sync(1)` / `syncfs(fd)` | 把**整台機器**／**整個檔案系統**的所有髒頁全部刷掉 | 範圍最大，通常不是程式該用的粒度 |
| `msync(addr, len, MS_SYNC)` | mmap 版的 fsync——把記憶體映射區裡改過的頁刷回底層檔案 | 對應第 4 節的 mmap |

**為什麼資料庫在乎這個**：資料庫的 ACID 裡「D（durability，持久性）」意思是「一旦回報 commit 成功，就算立刻斷電，資料也不會丟」。如果資料庫只呼叫 `write()` 就回報 commit 成功，那寫入其實還停在 page cache 裡當髒頁——斷電就沒了，違反 durability。所以 PostgreSQL 的 WAL、SQLite 的 journal，每次 commit 前都會 `fsync`（或至少 `fdatasync`）那段 log，**用犧牲一點延遲換「commit 說到做到」**。這也是為什麼資料庫調參數常看到 `fsync=off`（測試環境求快、犧牲耐久性）這種選項。

## 4. mmap 與 page cache 的關係

承 [背景-檔案動詞與process與mmap.md](背景-檔案動詞與process與mmap.md) 第三節：`mmap` 到底「映」的是什麼？

**答案：就是 page cache 本身的那些頁，直接映進你的位址空間。**

- 第一次存取 mmap 出來的位址會觸發 page fault；核心這時才把對應的檔案頁讀進 page cache（沒有的話），然後把你的 page table entry 指向那塊實體記憶體。
- 之後你用指標讀寫，走的是 CPU 的 MMU，**完全不經過 read/write 系統呼叫**，也不再經過 VFS——但你摸到的，跟另一個用 `read()` 讀同一檔案的 process 摸到的，其實是**同一份實體頁**（`MAP_SHARED` 下）。
- 這也解釋了「一般 read/write」跟「mmap」不是兩套獨立的快取：page cache 是共用的底層，read/write 是「透過系統呼叫搬一份資料出去」，mmap 是「把那塊記憶體本身借給你，連搬都不搬」。
- 對 `MAP_SHARED` 的頁，改動一樣先變成 dirty page，一樣要靠 writeback（或顯式 `msync`）才會真的落盤——mmap 沒有繞過 dirty/writeback 這套機制，只是繞過了「每次存取都要系統呼叫」這件事。

## 5. FUSE 的快取坑（對 unipath 直接相關）

FUSE 上核心疊了**兩層獨立的快取**，都可能讓你的合成檔案「看起來」比實際更新得慢：

| 快取層 | 快取什麼 | 預設行為 |
|---|---|---|
| **attr cache（屬性快取）** | `getattr`/`lookup` 回傳的 stat 結果（mtime、大小、權限…） | libfuse 高階 API 預設 `attr_timeout`/`entry_timeout` 約 **1 秒**；timeout 內核心直接用舊值，不再呼叫你的 `getattr` |
| **page cache（資料快取）** | `read` 讀到的檔案內容頁 | 未設 `direct_io`/`kernel_cache` 時：核心在每次 `open(2)` 靠 mtime/size 判斷「檔案是否變過」——若判斷沒變（可能就是因為那個判斷本身依賴的 attr 還在 attr cache 裡沒過期），就沿用舊的快取頁，**不會**真的呼叫你的 `read()` |

串起來看：一次 `cat mnt/0/data` 的旅程裡，核心先問「這檔案的中繼資料變了嗎」（可能命中 attr cache、可能真的呼叫你的 `getattr`），再決定要不要 invalidate page cache、真的呼叫你的 `read()`。

**unipath 目前能看到 counter 隨時間變**，不是因為每次 read 都繞過了快取——而是因為 attr cache 的 timeout 只有約 1 秒，短到「感覺即時」：每次 `cat` 只要間隔超過那個 timeout，核心就會重新問一次 getattr、看到變化、invalidate page cache、重新呼叫你的合成 `read()`。**這仍然是快取，只是快取窗口短到肉眼看不出破綻。**

若要**嚴格即時**（保證每次 read 都真的落到你的回呼、不被任何一層快取攔下）：

- 在 `open()` 回呼裡把 `fi.direct_io = True`：徹底繞過 page cache——每個 read/write 都直接進你的回呼，**沒有 readahead**，而且會**停用 shared mmap**（呼應第 4 節：direct_io 跟 mmap 天生衝突，因為 mmap 依賴的正是 page cache 那塊實體頁）。
- 把 `attr_timeout`/`entry_timeout` 設為 0：每次都重新問你的 `getattr`，不吃舊快取判斷「有沒有變」。

這正是 [背景-fuse與vfs.md](背景-fuse與vfs.md) §7 那句話的完整版本：**「執行態非快照」是 unipath 的語意承諾，但 FUSE 預設疊的兩層快取，會讓這個承諾在毫秒到一秒的窗口內悄悄跳票。**

## 6. 接 unipath / 快慢世界

Page cache 的本質就是一句話：**用記憶體換速度**，代價是「你讀到的不保證是此刻的真值，而是『上次快取建立時』的值，直到下次 invalidate」。這跟世界篇的快世界／慢世界軸線正好對上：

- **快世界**（在乎效能、可以忽略「這是不是此刻真值」）：page cache／FUSE 屬性快取／mmap 全部活在這裡——都是「先信任內容不常變，用快取抄近路」的優化。
- **慢世界**（unipath 現在站的地方，見 [背景-檔案動詞與process與mmap.md](背景-檔案動詞與process與mmap.md) 結尾）：每次存取都該是可攔截、可即時反映底層變化的訊息，「讀到的要是此刻真值」正是它的核心賣點。

問題在於：unipath 雖然自己（`unipath_live.py`/`unipath_pub.py`）每次 `read`/`getattr` 回呼都是**即時現算**，沒有任何底層磁碟延遲——但它是**掛在 FUSE 上**的，而 FUSE 預設幫你疊了 attr cache 與 page cache 這兩層「快世界式」的優化。也就是說：**你的合成邏輯本身活在慢世界，但傳輸它出去的管道（核心 VFS）預設用快世界的假設在跑**。兩者不對齊，就是 §5 那個「counter 看起來即時、其實只是快取窗口短」的縫隙。

日後做 tick／事件（`背景-fuse與vfs.md` §7 提過的伏筆）如果要嚴格保證「每個讀者都拿到当下這一刻的值」，`direct_io` 或關掉 attr cache 不是可選項，是必需品——否則 unipath 對外宣稱的「執行態非快照」，在核心的快取層面前只是一個時間窗口小到不容易被戳破的假象。
