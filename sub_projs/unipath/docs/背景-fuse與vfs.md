# 背景知識：FUSE 與 VFS 底層

> unipath 的先驗知識補充。理解 FUSE 底層 → 回頭秒懂 step 4 的 9P，因為兩者是同一個模式。
> 承 [OVERVIEW.md](../OVERVIEW.md)。

## 0. 先立柱子：VFS（虛擬檔案系統）

核心裡有一層 **VFS**。你的 `open`/`read`/`write`/`readdir` 系統呼叫**全部先進 VFS**。
VFS 不自己實作檔案系統，它是**統一介面 + 分派器**：

- 路徑在 `ext4` 分區 → 呼叫 ext4 的實作
- 在 `btrfs` → 呼叫 btrfs 的實作
- 在 `/proc` → 呼叫 procfs（無磁碟，即時合成）

**核心領悟：「檔案系統」＝一組回呼函數**（怎麼 getattr/read/readdir…）。VFS 定義介面，各家填實作。
`cat` 不在乎背後是磁碟還是合成——它只跟 VFS 講話。**這就是「一切皆檔案」的機關**：填得出那組回呼，任何東西都能扮成檔案。

## 1. 一般 fs vs FUSE

那組回呼**跑在核心裡**——寫核心程式難、崩了整台當、載入要 root。

**FUSE 的把戲**：`fuse.ko` 向 VFS 註冊成一種 fs，但**自己不實作** read/write，而是把每個請求**轉發到使用者態一支普通程式**。

```
一般 fs：  VFS ──呼叫──▶ ext4 回呼（核心內）──▶ 磁碟
FUSE：     VFS ──呼叫──▶ fuse.ko ──轉發訊息──▶ 你的程式（使用者態）──▶ 隨便你
```

## 2. 四個角色

| 角色 | 是什麼 |
|---|---|
| `fuse.ko` | 核心模組，向 VFS 註冊 `fuse` 類型 |
| `/dev/fuse` | 特殊字元裝置——**核心與你程式之間的訊息管道**（FUSE 命脈） |
| libfuse | 使用者態函式庫；讀 `/dev/fuse`、解析請求、呼叫你的回呼、寫回回應。**fusepy 用 ctypes 綁它** |
| `fusermount` | **setuid-root** 小工具，替非特權使用者做真正的 `mount()` |

## 3. 一次 `cat mnt/0/data` 的完整旅程

```
  cat：read("mnt/0/data")            ← 普通系統呼叫
    ▼
  VFS：路徑在 fuse 掛載點 → 分派給 fuse.ko
    ▼
  fuse.ko：打包成 FUSE_READ 請求，寫進 /dev/fuse
    ▼  (你的程式 read /dev/fuse 讀到)
  你的 daemon（libfuse 迴圈）：呼叫你的 read() 回呼 → 回傳 b"1\n" → write 回 /dev/fuse
    ▼
  fuse.ko：收到回應 → 交還 VFS → VFS 回給 cat
    ▼
  cat 印出 "1"
```

**`/dev/fuse` 是雙向訊息管道**：核心把「有人要 read 這路徑」寫進去，你的程式讀出、處理、寫回答案。
FUSE 在這管道上跑自己的**二進位協議**（`FUSE_LOOKUP`/`FUSE_GETATTR`/`FUSE_OPEN`/`FUSE_READ`/`FUSE_READDIR`…）。

## 4. 為什麼 FUSE 不用 root（但真 9P mount 要）

`mount()` 需要 `CAP_SYS_ADMIN`（root）。FUSE 怎麼繞：

**`fusermount` 是 setuid-root**。流程：
1. 非特權程式想掛載 → 呼叫 `fusermount`
2. `fusermount`（此刻 root）開 `/dev/fuse` 拿 fd、用它做真 `mount()`
3. 把那個 fd **透過 Unix socket 傳回**給非特權程式（`SCM_RIGHTS`——socket 能傳 fd）
4. 之後程式靠這 fd 跟核心通訊，全程非特權

**對照真 9P**：沒有 setuid 幫手，直接叫核心 `v9fs` `mount()` → **需要 sudo**。這就是為何 FUSE 步驟隨手跑、9P 步驟要敲 sudo。

## 5. 接回程式碼

- **fusepy** ＝ ctypes 呼叫 libfuse 的純 Python 綁定（沒編譯、沒 C）。
- `class UnipathFS(Operations)` 的 `getattr`/`readdir`/`read`/`write` ＝ §3 圖裡「你的回呼」。libfuse 從 `/dev/fuse` 解出 `FUSE_READ` 就呼叫你的 `read()`。
- `foreground=True` ＝ 別 daemon 化、讀迴圈跑前景（看 log）。`nothreads=True` ＝ 單執行緒、省鎖。

## 6. 最大洞見：FUSE 與 9P 是同一模式

```
FUSE：  核心 VFS  ◀──[FUSE 協議 / 經 /dev/fuse]──▶  使用者態 server
9P：    核心 VFS  ◀──[9P 協議  / 經 socket    ]──▶  使用者態 server
```

兩者都是「**核心 VFS 把檔案操作當訊息，轉發給使用者態 server 回答**」。差別只在協議與管道：

- **FUSE**：私有二進位協議、走 `/dev/fuse`、僅本機、setuid 免 root
- **9P**：公開標準協議、走**任意 socket（含 TCP）**、**能跨機器**、mount 要 root

所以「FUSE 起步 → 轉真 9P」＝**同一件事換更通用的協議**。9P 能跨機器，正是「兩世界通聯」的來源。

## 7. 相關的坑：快取

核心對 FUSE 有**快取**（屬性快取、page cache）。「執行態 counter」能讀到變化，是因預設快取超時很短（約 1 秒）；
若要嚴格即時，得開 `direct_io`／關屬性快取。這跟「執行態非快照」直接相關，做 tick 會再遇到。
