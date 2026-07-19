# 背景知識：一個檔案能做啥動作？當它是 process / mmap 又如何？

> unipath 先驗知識。承 [背景-fuse與vfs.md](背景-fuse與vfs.md)。
> 本篇兩問：(1) read/write 之外檔案還有哪些動詞？Unix vs Plan 9 的取捨。(2) 當「檔案」其實是一個 process 或一塊 mmap 記憶體，語意怎麼變。

## 一、一個檔案 ＝ 它回答哪些「動詞」

VFS 裡每個檔案背後掛著動詞表（`file_operations`/`inode_operations`）。動詞分**兩大家族**：

- **作用在「內容 / 開啟的 fd」**：read、write、lseek、mmap、ioctl…（先 `open` 拿 fd）
- **作用在「名字 / inode 中繼資料」**：rename、chmod、stat、unlink…（作用在路徑，不必開它）

| 家族 | 動作 | 幹嘛 |
|---|---|---|
| 內容傳輸 | `read` `write` `pread`/`pwrite` `readv`/`writev` `sendfile` `splice` `copy_file_range` | 搬位元組 |
| 定位 | `lseek`（SEEK_SET/CUR/END/DATA/HOLE） | 移游標、找洞 |
| 記憶體映射 | `mmap` | 映進位址空間，用指標存取 |
| 裁切/預配 | `truncate`/`ftruncate` `fallocate` | 改大小、預配空間 |
| 中繼資料讀 | `stat`/`fstat`/`statx` | 大小/時間/權限/擁有者/inode |
| 中繼資料寫 | `chmod` `chown` `utimensat` | 改權限/擁有者/時間 |
| 控制（帶外） | `ioctl` `fcntl` | read/write 表達不了的雜項控制 |
| 鎖 | `flock` `fcntl(F_SETLK)` `lease` | 協調多方存取 |
| 同步 | `fsync` `fdatasync` `msync` | 刷快取到底層 |
| 擴充屬性 | `getxattr`/`setxattr`/`listxattr`/`removexattr` | 在檔案上掛任意 key-value 中繼資料 |
| 事件/就緒 | `poll`/`select`/`epoll` `inotify` | 問「可讀了嗎」、監看變動 |
| 名字空間 | `link` `symlink` `rename` `unlink` | 建連結/改名/刪 |
| 目錄專屬 | `readdir`/`getdents` `mkdir` `rmdir` | 列/建/刪目錄 |
| fd 本身 | `dup`/`dup2`、經 unix socket `SCM_RIGHTS` 傳 fd | 複製、跨 process 傳遞 |

### 三個「read/write 以外」的明星

- **`ioctl`**：帶外控制的雜物抽屜（私有魔數、不可 walk）。**Plan 9 廢掉它，改寫 `ctl` 檔**——unipath 抄的正是這慣例。
- **`xattr`**：檔案上掛 key-value 標籤（`user.author=…`）。Plan 9 也沒有，寧可多開約定檔（如 `status`）。＝「多動詞 vs 多檔案」取捨。
- **`mmap`**：把檔案當記憶體、指標存取（見下第三節）。

### 關鍵對照：Unix 動詞爆炸 vs Plan 9 刻意極簡

- **Unix/Linux**：幾十個動詞 + `ioctl` 無限擴充口。強大但雜。
- **Plan 9 / 9P**：砍到只剩 `walk`/`open`/`read`/`write`/`clunk`/`stat`/`wstat`/`create`/`remove`。要控制→寫 `ctl`、要狀態→讀 `status`。

> **Unix 把複雜性放進動詞；Plan 9 把複雜性放進命名空間（檔案樹）。** 選 9P ＝ 選「動詞極簡、語意靠檔案約定表達」。

## 二、當「檔案」其實是一個 process

有兩層意思，都成立：

### 2.1 process 的狀態被暴露成檔案（/proc 式）

`/proc/<pid>/` 就是一個 process 攤成一疊檔案：

- `status`/`stat`/`cmdline`/`environ` — 讀它的狀態
- `fd/` — 它開的每個 fd（symlink）
- `maps` — 它的記憶體映射表
- **`mem` — 它的整個位址空間當成一個檔案**：`lseek` 到某虛擬位址、`read`/`write` 就讀寫**另一個 process 的記憶體**（需 ptrace 權限）
- `ns/` — 它的 namespaces

**Plan 9 更乾淨**：`/proc/<pid>/ctl` 寫 `kill`/`stop`/`start` 就控制它；`note` 是信號（字串）。**控制一個 process ＝ 寫它的檔案。**

現代還有 **`pidfd`**：把整個 process 當一個 fd 持有——`pidfd_send_signal` 發信號、`poll` 它等它結束、`pidfd_getfd` 偷它的 fd。

### 2.2 process 就是那個 file server（合成檔案）

這才是 unipath / Plan 9 的核心：**「檔案」根本不是儲存，是一支跑著的程式即時合成的介面。**

於是動詞的語意整個變了：

> **當檔案背後是 process，`read`/`write` 就變成對那個 process 的 RPC / 訊息傳遞。**
> `read` ＝「問這 process 當前值」，`write` ＝「送這 process 一個命令/資料」，語意由 process 自己定義。

這正是我們 `unipath_pub.py` 做的事——`read mnt/0/data` 被派發成呼叫發布端的 handler。

**還解鎖一個新維度**：process 背後的檔案可以**阻塞 / 串流**。讀一個 pipe 會卡住直到有人寫；讀 Plan 9 的 `/net/tcp/…/data` 會卡到網路有資料。所以 `read` 不只是「取快照」，可以是「**等下一個事件**」——這時 `poll`/`select` 才有意義（問：有資料了嗎）。unipath 目前是快照式 read，日後做 tick/事件會用到這個。

## 三、當「檔案」是 mmap / 共享記憶體

`mmap(fd)` 把檔案內容映進你的位址空間，之後**用指標直接存取、不再呼叫 read/write**：

- **一般檔案 + `MAP_SHARED`**：改記憶體 → 核心刷回檔案；多個 process 映同一檔 → 共享同一份實體頁。
- **純共享記憶體**：`shm_open`（產生 `/dev/shm/<名>`）或 `memfd_create`（匿名）→ mmap → 多 process 共享同一份 RAM 頁。**沒有 read/write 呼叫、沒有拷貝——最快的 IPC。**

### 這裡有個根本張力，直接關係你的設計

**read/write 模型**：每一次存取都是一則**訊息**，你的 server（或核心）都看得到、攔得到、可跨機器轉發。
**mmap 模型**：一旦映好，client 直接摸記憶體，**你的 server 看不到每次存取**（FUSE 下 server 只在**頁層級**的 fault/flush 被叫到，不是每個位元組）。

| | read/write（9P 式） | mmap / 共享記憶體 |
|---|---|---|
| 每次存取 | 是一則可攔截的訊息 | server 看不到（只頁層級 fault/flush） |
| 速度 | 慢（一趟趟系統呼叫/RPC） | 快（零拷貝、直接摸記憶體） |
| 可否跨機器 | **可**（9P over TCP、兩世界通聯） | **不可**（共享記憶體本質是本機同一份實體頁） |
| 可否 tick/攔截/合成 | 可（每次都經你手） | 難（旁路了你） |

> **一句話**：mmap 用「放棄可攔截性 + 放棄跨機器」換「零拷貝的速度」。這就是為什麼 **Plan 9 幾乎不用 mmap**——它要的是「每個存取都是可 walk、可轉發、網路透明的訊息」，mmap 恰恰違反這點。

### 接回你筆記的「快世界 / 慢世界」

這張對照正好落在你世界篇的軸上：

- **慢世界 ＝ read/write / 9P**：每步可攔截、可 tick、網路透明、能跨世界通聯。**忽略 CPU 週期**沒關係。
- **快世界 ＝ mmap / 共享記憶體**：零拷貝、在乎效能、但本機且不可攔截。

所以「檔案是 process 還是 mmap」不是二選一，而是**兩個世界各自的預設交互形式**：
- 要**可控、可觀測、可跨機器**（慢世界／規範層）→ 走 process-backed read/write（9P）。
- 要**極速、本機、大量資料**（快世界）→ 走 mmap 那塊，代價是那段對外「不透明」。

unipath 現在整個站在**慢世界**（read/write/9P）。快世界的 mmap 是日後某個元素需要飆效能時，才在那個局部開的逃生門——且要在兩世界通聯協議裡標清楚邊界（見兩層底座篇 §三）。
