# 背景知識：fd 操作三組（控制 / 等就緒 / 複製遞交）

> unipath 先驗知識。承 [背景-檔案動詞與process與mmap.md](背景-檔案動詞與process與mmap.md)。
> 三組 syscall 的共同主題：**它們不搬位元組，而是操作「fd 這個抽象」本身**。

## 前提：fd（檔案描述符）是什麼

fd 是一個**小整數**，是你 process 裡「開啟檔案表」的索引。每個表項指向核心裡一個
**open file description**（開啟描述）：存讀寫游標、旗標（如 O_NONBLOCK）、並指向真正的檔案/socket/pipe。

```
你的 process                核心
┌──────────────┐
│ fd 0 ──────────────▶ open file description ──▶ 某個 pipe
│ fd 1 ──────────────▶ open file description ──▶ 終端機
│ fd 3 ──────────────▶ open file description ──▶ /home/…/a.txt（游標=100）
└──────────────┘
```

下面三組全是在動這張圖。**dup 共享同一個 open file description（共享游標）；重新 open 則是另一個獨立描述。**

## 一、ioctl / fcntl —— 控制這個 fd（搬位元組以外）

**`ioctl(fd, 命令碼, 參數)` —— 裝置專屬的雜物抽屜**
每種驅動自定義命令碼，**無型別**（魔數 + 指標，核心自解）。例：
- `TIOCGWINSZ`：問終端機幾行幾列（vim 靠這知道視窗大小）
- 設序列埠波特率、網卡 IP（`SIOCGIFADDR`）、光碟退片

私有魔數、不可 walk、不可讀 → **Plan 9 廢掉它，改「寫 `ctl` 檔」**（unipath 用的正解）。

**`fcntl(fd, cmd, 參數)` —— 通用的 fd/開啟描述控制**（比 ioctl 標準化）
- `F_GETFL`/`F_SETFL`：改旗標，最常用設 **`O_NONBLOCK`**（read 沒資料立刻返回、不卡）
- `F_GETFD`/`F_SETFD`：設 **`FD_CLOEXEC`**（exec 時自動關此 fd）
- `F_SETLK`/`F_SETLKW`：**檔案鎖**
- `F_DUPFD`：複製 fd
- `F_SETOWN`+`O_ASYNC`：此 fd 有資料時發 `SIGIO`

> ioctl ＝裝置各自的無限擴充口；fcntl ＝一組標準 fd 控制動詞。兩者皆控制通道、與內容無關。

## 二、poll / select / epoll —— 同時等一堆 fd 誰就緒

**問題**：手上 100 個 socket，想「誰有資料處理誰」。但 `read` 一個會**卡住**、看不了其他；每 fd 一條 thread 太重。需要「幫我盯著這堆 fd，誰能讀了叫我」。

三代演進，同一件事：

| | 用法 | 極限 |
|---|---|---|
| `select` | 傳三個 bitmap（讀/寫/例外），返回誰就緒 | 最多 1024 fd、每次 O(n) 掃、每次重建 bitmap |
| `poll` | 傳 `{fd, 想要事件}` 陣列 | 無 1024 限制，但仍每次 O(n) 掃全部 |
| `epoll`（Linux 專屬） | `epoll_ctl` **註冊一次**，`epoll_wait` **只回就緒的** | 撐 10 萬 fd、近 O(1)；nginx/node.js 底座 |

回答的都是：「這堆 fd 裡，哪些現在可 read/write 而不卡？」＝所有**事件迴圈**的核心。

**接前篇**：process 背後的 read 可阻塞、變「等下一個事件」；`poll`/`epoll` 就是**告訴你何時該去 read**。
而 `timerfd`/`signalfd`/`inotify` 的價值正是**能被 epoll 統一盯著**——一個迴圈同時等「網路來資料、計時器到、收信號、某檔被改」，全化約成「某 fd 就緒」。這就是「萬物 fd 化好讓 poll 統一」。unipath 日後做 tick/事件驅動會用到整套。

## 三、dup / dup2 / SCM_RIGHTS —— 複製 / 搬移 fd

**`dup(oldfd)`**：給一個**新 fd 號碼**，指向**同一個 open file description**（共享游標/旗標）。

**`dup2(oldfd, newfd)`**：同上但**你指定目標號碼**（newfd 已開先關）。＝**shell 重導向的機關**：
```
command > out.txt
```
＝ `open("out.txt")`→fd 3 → `dup2(3, 1)` 把 **stdout(fd 1)** 改指向該檔 → `exec command`。
command 以為在寫螢幕，其實寫進檔案。`a | b` 也是用 dup2 把兩端接上 pipe。

**`SCM_RIGHTS` —— 把活 fd 傳給「另一個 process」**
前兩個在同 process 內複製；SCM_RIGHTS 是**跨 process**：透過 Unix domain socket `sendmsg` 夾帶特殊控制資料，把 fd 送給對方；核心在**對方 fd 表**複製出同一個 open file description（號碼可不同、指向同一核心物件）。

把一個**已開啟、活的資源**（開著的檔、連著的 socket、`/dev/fuse` handle）整個交到另一 process 手上。用途：
- **就是 `fusermount` 的把戲**：它（root）開好並 mount `/dev/fuse`，把 fd 用 SCM_RIGHTS 傳回非特權 daemon（FUSE 免 root 的關鍵）。
- 權限分離：root 小幫手開敏感資源、把 fd 丟給沙箱 worker。
- **接 unipath**：「取元素＝拿 fd」若要跨 process 真交出**能力**（非只 RPC 轉發），SCM_RIGHTS 就是「把活 fd 遞過去」的機制。

## 串起來

fd 是核心資源的把手：**ioctl/fcntl 控制它、poll/epoll 等它就緒、dup/SCM_RIGHTS 複製或遞交它**。
搬位元組（read/write）之外，一個 fd 的完整生命就這些動作。
