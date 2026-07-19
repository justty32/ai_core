# 背景知識：inode/dentry 與 pipe/socket

> unipath 先驗知識。承 [背景-fd操作三組.md](背景-fd操作三組.md)。
> 搞懂這兩組 → fd、stat、9P 的 walk、pipe、socket 一次串成一張圖。

## 一、inode / dentry —— 「檔案」與「名字」是分開的

Unix 檔案系統最反直覺、也最關鍵的一點：

**inode（索引節點）＝ 真正的「檔案」**
存這檔案的一切——大小、權限、擁有者、時間戳、連結數、**指向資料區塊的指標**——但**不存名字**。
每個檔案 ＝ 一個 inode；inode 號是它在該檔案系統的真實身份。

**dentry（目錄項）＝ 「名字 → inode」的對應**
一個目錄本質就是一串 dentry（名字 → inode 號）。dentry 串成樹（各自知道父/名字/指向的 inode），核心快取它們（dcache）加速查找。

> **核心領悟：檔案沒有名字。** 名字住在目錄（dentry），inode 才是本體；名字只是指向 inode 的參照。

這一刀讓很多怪事合理：

- **硬連結**：兩個名字指向**同一 inode**。故 inode 有「連結數」——`rm` 只刪一個名字 + 連結數 -1，**資料要等連結數歸 0 且沒人開著才真釋放**。
- **`rename`**：只動 dentry，inode/資料不動。
- **路徑查找**（namei）：從 `/` 或 cwd 的 dentry 出發，逐段在父目錄 inode 裡查下一個 dentry → 子 inode → 再查。

### 三層 many-to-one（把 dup 也接上）

```
fd ──▶ open file description（游標/旗標）──▶ inode（檔案本體）
名字(dentry) ─────────────────────────────▶ inode
```

- 多個 fd → 同一 open description（`dup`）
- 多個 open description → 同一 inode（分別 open 兩次）
- 多個名字 → 同一 inode（硬連結）

### 接 unipath

- step 4 的 **`qid` ＝ 9P 版 inode 身份**（唯一 id + 型別）
- **`Twalk` ＝ dentry 查找**
- 我們建的 **stat 結構 ＝ inode 中繼資料**
- `Env` 的「路徑 → 物件」導航 ＝ **即時生成 dentry、每個物件扮 inode**

> 你早就實作了一套 inode/dentry 模型，只是沒用這名字。procfs/FUSE 同理：合成式檔案系統＝按需生成 inode 與 dentry。

## 二、pipe / socket —— 兩個 process 在核心裡怎麼接起來

**`pipe()` —— 最小的單向管子**
呼叫一次給**兩個 fd**（讀端、寫端），中間夾核心緩衝區（約 64KB 位元組佇列）。單向。

- 匿名 pipe 靠 **fork 繼承 fd** 分享——shell 接 `a | b` ＝ `pipe()`→`fork`→子 process `dup2` 把 stdout/stdin 接到兩端。
- **具名 pipe（FIFO）**：`mkfifo` 給它檔案系統路徑，不相干 process 也能按名開。
- 語意 ＝「阻塞式 read＝等事件」：讀端沒資料就卡；所有寫端關 → 讀到 **EOF**；緩衝滿寫端卡；沒讀者還寫 → `SIGPIPE`。pipe 有匿名 inode、資料在核心緩衝、無磁碟。

**`socket()` —— 通用雙向端點**

- **`AF_UNIX`（本機）**：同機兩 process，用**檔案系統路徑**定址；雙向、可靠、能 `SCM_RIGHTS` 傳 fd/憑證。**`unipath_pub.py` 用這個。**
- **`AF_INET`（網路）**：TCP/UDP over IP，**能跨機器**。**`unipath_9p.py` 用這個（TCP）。**
- 連線流程（串流）：
  ```
  server: socket() → bind(位址) → listen() → accept()   ← 每條連線回一個新 fd
  client: socket() → connect(位址)
  ```
  之後雙方 `read`/`write` 連好的 fd，像 pipe 但**雙向**。
- 跟 pipe 的差別：socket 雙向、可連網、有位址、能 `accept` 多 client；pipe 是本機單向小表弟。

### 接 unipath（關鍵洞見）

steps 3/4 的水管就是這個。同一套「walk/read/write over socket」＝ 9P：

- 走 **AF_UNIX** → 本機（step 3 pub）
- 一模一樣的邏輯走 **AF_INET/TCP** → **跨機器 ＝ 兩世界通聯**（step 4）

> 這就是 9P「跟傳輸無關」為何重要——換個 socket 型別，本機協議直接變跨機器協議，程式碼不用改。

## 收攏

pipe、socket、一般檔、timerfd… 核心裡是不同東西，但**全露成 fd**。
於是 `read`/`write`/`poll`/`dup`/`ioctl` 一視同仁——**這就是「一切皆檔案（描述符）」的最大紅利**：
一個事件迴圈用同一套動詞同時對付管子/網路/計時器/檔案變動。
unipath 的野心 ＝ 把這紅利從「核心給的那些東西」擴張到「**任意執行態物件**」。
