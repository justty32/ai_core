# 背景知識：mount 命名空間與 bind

> unipath 先驗知識。承 [背景-inode與pipe與socket.md](背景-inode與pipe與socket.md)。
> 上一篇說「檔案沒有名字，名字住在目錄（dentry），inode 才是本體」；mount 把這套推到更大尺度——
> **把一整棵目錄樹接到某個 dentry 位置上**，資料不動，動的是「從這裡往下查，查到誰」。

## 一、mount()——覆蓋一個目錄

**`mount(source, target, filesystemtype, mountflags, data)`**：把 `source`（設備、或另一棵子樹、或什麼都不重要如 tmpfs）
接到 `target`（必須是已存在的目錄，即**掛載點**）之後，VFS 解析路徑碰到 `target` 這個 dentry 時，
會**改道**走進新掛上的檔案系統的根 dentry。

```
掛載前：/mnt/usb/               （空目錄，或裡面有舊東西）
掛載：  mount /dev/sdb1 /mnt/usb
掛載後：/mnt/usb/  ──▶ 蓋住了 ──▶ 現在查到的是 /dev/sdb1 檔案系統的根
                         （/mnt/usb 原本若有內容，此刻走不到，但沒被刪，umount 後現形）
umount /mnt/usb                  ← 蓋子掀開，打回原狀
```

**核心：mount 不搬資料、不複製檔案**，只是在 dentry 樹某一點換一顆子樹接上去——跟前篇「名字≠本體」
是同一招數放大尺度：掛載點這個 dentry 本身沒變，變的是「再往下查，查到誰的樹」。

一台機器看到的完整路徑樹，其實是好幾個獨立檔案系統疊出來的拼接視圖：`/` 是第一個 mount（根檔案系統），
`/home`、`/boot`、`/proc`、`/sys` 常是各自獨立 mount 上去的東西（分區、或合成式 fs）。
`/proc/self/mounts`（或 `/proc/mounts`）就是目前這個 process 看到的完整 mount 表：來源、掛載點、型別、選項。

## 二、bind mount——同一份 inode，兩個位置

**`mount --bind 舊路徑 新路徑`**（syscall 版：`mount(source, target, NULL, MS_BIND, NULL)`）：
不是掛一個新檔案系統，而是把**既有一棵子樹**再掛一份到另一個路徑——效果是**同一份 inode 現在能從兩個路徑走到**。

```
mkdir /mnt/proj_view
mount --bind /home/me/project /mnt/proj_view
echo hi > /mnt/proj_view/a.txt
cat /home/me/project/a.txt        # 印出 hi —— 兩個路徑，同一個檔案
```

跟硬連結的差別：硬連結是「多個名字指到同一 inode」（同一檔案系統內，dentry 層的事）；
bind mount 是**整個目錄子樹**在另一位置現形，可以跨掛載點、看起來像本來就長在那裡的一部分，
也可以只 bind 單一檔案。`mount --rbind`（遞迴版）連子路徑下原本已掛好的東西一起帶過去。

唯讀變體：先 bind，再 `mount -o remount,bind,ro`——做出「同一份資料，這邊可寫、那邊唯讀」的兩個視圖。

用途：容器掛 volume（host 目錄 bind 進 container 看到的特定路徑）；chroot 環境裡借用 host 的
`/dev`、`/proc` 常用 bind mount，比整個重造一份輕得多。

## 三、overlay / union mount——多層疊成一個視圖

前兩者是「一對一」的位置代換；overlay 是「多對一」：把好幾層目錄疊起來，變成一個合併視圖。

Linux 的 **OverlayFS**：`lowerdir`（唯讀，可用 `:` 疊多層）＋ `upperdir`（可寫）＋ `workdir`（暫存區，
須跟 upperdir 同檔案系統）→ 掛成一個 `merged` 目錄：

```
mount -t overlay overlay \
  -o lowerdir=/base,upperdir=/changes,workdir=/work \
  /merged
```

| 動作 | 行為 |
|---|---|
| 讀 `a.txt` | 先查 upperdir 有沒有這名字 → 沒有才往 lowerdir 找（由上往下疊） |
| 寫一個只存在 lowerdir 的檔 | **copy-up**：整份先複製進 upperdir，之後改動都落在 upperdir 上，lowerdir 原檔不動 |
| 刪一個只存在 lowerdir 的檔 | lowerdir 不動；upperdir 留一個 **whiteout** 標記，蓋住它讓合併視圖看不到 |

這就是 **Docker image 分層**的底座：每層 image ＝ 一個唯讀 lowerdir，容器的可寫層 ＝ upperdir；
十個 container 能共用同一組唯讀層，只有各自改動落在自己的 upperdir——省空間、啟動快的祕密。

union mount 的概念更早見於 **Plan 9**（`bind` 的 union 模式，見 §五）與 BSD 的 unionfs：
把多個來源疊在同一個掛載點，走名字查找時依序找過去，第一個命中的算數。OverlayFS 是這個老概念的
Linux 落地版，只是多了 copy-up／whiteout 這些為了「唯讀層真的不能動」而生的機關。

## 四、mount 命名空間（CLONE_NEWNS）——每個 process 自己的檔案系統視圖

到此為止，mount 表（一台機器上「誰掛在哪」）預設是**全機器唯一、所有 process 共用**的一張表——
`mount`/`umount` 一次全域生效。**mount namespace** 打破這個假設：

`clone(CLONE_NEWNS)` 或 `unshare(CLONE_NEWNS)`（指令列：`unshare --mount`）會**複製一份目前的 mount 表**
給新 namespace；此後在新 namespace 裡 `mount`/`umount`，只影響這個 namespace 看到的樹，
不會動到外面（也不會被外面動到）——除非用 mount propagation（`shared`/`private`/`slave`/`unbindable`，
`mount --make-*` 設定）明確要求互相同步。

> **小知識**：Linux 現有 8 種 namespace（mount／PID／net／UTS／IPC／user／cgroup／time），
> 但 mount namespace 是**最早**加進去的（2002 年，kernel 2.4.19）——那時還沒有「namespace 家族」的概念，
> 所以它的 flag 叫 `CLONE_NEWNS`（就是「新命名空間」，沒有 `MNT` 字樣）；後來加入的才照
> `CLONE_NEWPID`/`CLONE_NEWNET`/`CLONE_NEWUSER`… 這種「NEW+種類」命名——`NEWNS` 是先來者的歷史包袱。

每個 mount namespace 對應 `/proc/<pid>/ns/mnt` 這個檔（本身也是個 inode；多個 process 若共用同一
namespace，這幾個檔會指到同一 inode）；`/proc/<pid>/mounts` 顯示該 process 所在 namespace 目前的
mount 表。`setns()` 能讓一個 process 加入既有的 namespace；`nsenter`/`ip netns` 就是靠 bind 住
`/proc/<pid>/ns/*` 這些檔讓 namespace 在無 process 時也不被回收。

**容器（Docker）隔離的基礎之一**：一個容器＝一組 namespace 疊起來（mount＋PID＋net＋UTS＋IPC＋user＋cgroup），
container runtime 用 `clone`/`unshare` 開好這組 namespace，再用 `pivot_root`（換根）＋一串 **bind mount**
（把 image 層、volume 一一接進去）＋ **overlay**（疊 image 層）組出容器看到的整棵樹——
「容器的檔案系統」不是變出一份新磁碟，是 **mount namespace ＋ bind mount ＋ overlay 三招疊出來的視圖**。

`mount()` 傳統上要 `CAP_SYS_ADMIN`（root）；rootless container 能繞過，靠的是先開一個 **user namespace**
（讓你在裡面「看起來」是 root、能操作 mount），再疊 mount namespace——這是 Docker 早期一定要 root、
近年才有 rootless 模式的技術原因。

## 五、接 Plan 9 / unipath——per-process namespace，更徹底的版本

Linux 的 mount namespace 是**事後用 clone flag 疊出來的特例**：預設所有 process 共用一張表，
要隔離得主動 `unshare` 出一份、還要湊齊一整組 namespace＋root（或 user namespace）才拼得出容器級隔離。

**Plan 9 從設計第一天就把「每個 process 有自己的命名空間」當預設，而非特例。**
Plan 9 沒有全域唯一的 mount 表：每個 process 的命名空間是它自己的一棵樹，由一連串 `bind`/`mount`/`unmount`
（都是**普通系統呼叫，非特權**）組成；`fork` 出的子 process 預設共享同一個「命名空間群組」，
除非明確 `rfork(RFNAMEG)` 要一份自己的獨立拷貝，之後互不影響。

| | Linux mount namespace | Plan 9 per-process namespace |
|---|---|---|
| 預設狀態 | 全機器一張表、所有 process 共用 | 每個 process 天生一份（或共享一個群組） |
| 要不要特權 | `mount()` 傳統要 `CAP_SYS_ADMIN`；rootless 靠疊 user namespace 繞 | `bind`/`mount` 對自己有權限碰的資源，一般使用者直接能做 |
| 隔離粒度 | 整組 namespace 一起開（容器＝一包） | 逐個掛載點客製化，粒度細到單一 bind |
| union/疊層 | 要另外掛 OverlayFS 才有 | `bind -a`（疊到後面）/`bind -b`（疊到前面）內建 union 語意 |
| 跟網路的關係 | mount 本身與網路無關，需另外設計分散式 fs | `bind`/`mount` 的來源可以直接是遠端 9P server，語法跟本機一樣 |

最後一行是關鍵：Plan 9 的 `bind` 天生跟 **9P 網路透明**合一——掛進命名空間的東西，可以是本機檔案，
也可以是遠端機器透過 9P 開的 file server，寫法完全一樣。這正是前篇「接 unipath」點過的事：
`AF_UNIX` 走本機、`AF_INET` 走跨機器，程式碼不用改；放到 mount 這層，就是「`bind` 一個本機目錄」
跟「`mount` 一個遠端 9P server」是同一個動詞。

**接 unipath**：`workflows/notes/20260719-1518-實作路線規劃-採plan9與9p蓋在linux-方案a.md` §三對照表
已把這條路標好——「借樹、每世界/NPC 自己的視角」對應的正是 **per-process namespace：每個世界＝一個
命名空間**。目前 unipath（step 1~4）是**一個全域 FUSE 掛載點、所有人看同一棵樹**——約略還停在
「Linux 傳統單一全域 mount 表」那個階段。日後要做「每個世界有自己的路徑視圖」，不必真的去碰 Linux
的 `CLONE_NEWNS`（太重、要 root/unshare、還要湊 pivot_root）；更貼近 unipath 已經在做的事（step 3/4
本來就是使用者態的 9P server/client）是**在 unipath 自己的 walk 邏輯裡山寨 Plan 9 那套**：
每個 client（每個世界）連上時自己維護一份「路徑前綴 → 對到哪個後端 Env」的對照表——這張表就是
使用者態版的命名空間，`bind` ＝往表裡加一條映射，`union` ＝同一路徑對到多個來源、依序查找。
這樣完全不必碰核心特權，跟 step 4 的 9P server 一樣留在使用者態就能做完。

## 收攏

mount 系列的所有花樣（bind／overlay／namespace）都在做同一件事：**在 dentry 樹的某個位置，換一顆
子樹接上去**，資料本身不動，動的是「從這裡往下查，查到誰」。容器＝疊好幾層這種換法——namespace
決定看到哪張 mount 表、bind 決定哪個目錄借哪份資料、overlay 決定唯讀層＋可寫層怎麼合併。
Plan 9 把這招從「核心特權操作、全機器共用一份」下放成「每個 process 都能日常使用的動詞」，
且跟 9P 網路協議天生合一——這正是 unipath 想抄的野心：不只把執行態物件變成路徑，還要讓
**「誰能看到哪些路徑」本身也是可以組裝的**（每個世界，自己一份命名空間）。
