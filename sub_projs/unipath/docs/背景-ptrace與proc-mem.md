# 背景知識：ptrace 與 `/proc/<pid>/mem` —— 操縱「別的 process」的極致

> unipath 先驗知識。承 [背景-inode與pipe與socket.md](背景-inode與pipe與socket.md)。
> 前幾篇都在講「自己這個 process 怎麼用檔案/fd 跟核心、跟別的 process 打交道」；本篇反過來問一個更兇的問題——
> **一個 process 能不能直接伸手進去，暫停、窺視、甚至改寫「另一個活著的 process」的內部**：
> 它的暫存器、它下一步要跑的系統呼叫、它整片記憶體？答案是 `ptrace` 這組機關；
> `/proc/<pid>/mem` 是它「檔案化」的出口，gdb / strace / 一切 debugger 全靠這個地基。

## 一、ptrace —— 附掛、暫停、窺視、攔截

```c
long ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data);
```

一個 syscall、一個總開關 `request` 決定要幹嘛。核心概念：**附掛（attach）之後，tracee 每次該「停下」的時刻都會真的停下**（收到訊號、跑到一個系統呼叫邊界、單步執行完一條指令……），
此時 tracer 用 `waitpid` 收到通知，可以在 tracee 完全靜止的狀態下窺視/竄改它，再放它繼續跑。

| request | 幹嘛 |
|---|---|
| `PTRACE_TRACEME` | tracee 自己喊的：「我父 process 是我的 tracer」——**gdb 直接跑一支程式**（`gdb ./a.out` 再 `run`）走這條：fork 出子 process、子呼叫這個、再 `execve`，exec 完立刻停一次讓你設中斷點 |
| `PTRACE_ATTACH` / `PTRACE_SEIZE` | tracer 主動附掛到**已在跑**的 pid（`gdb -p <pid>`、`strace -p <pid>`）；`ATTACH` 會立刻送 SIGSTOP 逼停，`SEIZE` 較新、不強制立即停、行為更可控 |
| `PTRACE_PEEKTEXT` / `PTRACE_PEEKDATA` | 讀 tracee 記憶體，**一次一個 word**（4 或 8 bytes）；TEXT/DATA 在現代 Linux 上完全等價，只是歷史包袱 |
| `PTRACE_POKETEXT` / `PTRACE_POKEDATA` | 寫 tracee 記憶體，同樣**一次一個 word** |
| `PTRACE_GETREGS` / `PTRACE_SETREGS` | 讀/寫 tracee 全部暫存器（`struct user_regs_struct`：rip、rsp、rax……） |
| `PTRACE_SYSCALL` | 「跑到下一個系統呼叫的進/出口就停」——tracee 每呼叫一次 syscall 就被攔兩次（進入前、返回後），這正是 **strace 攔截並印出每個系統呼叫的機制** |
| `PTRACE_CONT` / `PTRACE_SINGLESTEP` | 放行繼續跑 / 只跑一條指令就再停 |
| `PTRACE_DETACH` | 解除附掛，tracee 恢復自由身 |

**PEEK/POKE 一次只搬一個 word**，要讀一整塊記憶體得**逐 word 迴圈呼叫 syscall**——又慢又囉唆。這正是下一節 `/proc/<pid>/mem` 存在的理由：把「一次一個 word 的 ptrace 呼叫」換成「一次一大段的 read/write」。

> **中斷點怎麼做**：gdb 用 `PTRACE_POKETEXT` 把目標位址那個 byte 換成 `0xCC`（x86 的 `int3` 指令），先存下原本的 byte。
> CPU 執行到 `int3` 會拋出例外 → tracee 停 → tracer 的 `waitpid` 醒來 → gdb 把該 byte 復原、印出當下暫存器/變數，等你 `continue` 才把原指令跑掉、再插回 `0xCC`。
> **一個 debugger 的全部魔法，說穿了就是「暫停 + 讀寫記憶體 + 讀寫暫存器」這三招的排列組合。**

## 二、`/proc/<pid>/mem` —— 把別人整個位址空間當一個檔案

上一篇提過這個檔案「需要 ptrace 權限」，這裡把底層機制攤開：

**用法就是普通檔案三步：**
```
fd = open("/proc/<pid>/mem", O_RDWR)
lseek(fd, 目標虛擬位址, SEEK_SET)   ← 位址當 offset 用
read(fd, buf, 長度)                 ← 讀那段記憶體
write(fd, buf, 長度)                ← 寫那段記憶體
```
沒有 word 上限、沒有迴圈——`read`/`write` 想搬多少就搬多少。開這個檔要通過跟 `PTRACE_ATTACH` 同一套權限檢查（見下節），實務上多半先 `PTRACE_ATTACH`（或本身就是它的 tracer）讓對方停在一個確定狀態，再開這檔案讀寫，否則讀寫一個正在跑、位址內容隨時飄移的 process，意義不大也容易 race。

**搭配 `/proc/<pid>/maps` 找位址——不然你怎麼知道要 lseek 去哪？**

```
address                  perms offset   dev   inode   pathname
55f2a1200000-55f2a1201000 r-xp 00000000 08:01 131074  /usr/bin/mytool
55f2a1401000-55f2a1422000 rw-p 00000000 00:00 0       [heap]
7f3c8c000000-7f3c8c021000 rw-p 00000000 00:00 0
7f3c8c1a0000-7f3c8c33a000 r-xp 00000000 08:01 262145  /usr/lib/libc.so.6
7ffce2a00000-7ffce2a21000 rw-p 00000000 00:00 0       [stack]
```
每行＝一段連續映射：位址範圍、權限（r/w/x/**p**=私有或**s**=共享）、在檔內的 offset、裝置、inode、來源檔案（`[heap]`/`[stack]`/匿名映射沒有 pathname）。
debugger 要「讀某個全域變數」，先在 `maps` 裡認出對的映射（比如 `libc.so.6` 那段的起始位址），加上符號在檔內的 offset，湊出目標虛擬位址，才知道要 lseek 去哪——**`mem` 負責搬資料、`maps` 負責告訴你資料在哪**，兩檔配對用。

**現代更快的替代品：`process_vm_readv`/`process_vm_writev`**——單一 syscall 做 scatter-gather 搬移（一次可指定多段來源/目的），不用 `open`+`lseek`+`read` 三趟系統呼叫，也不用逐 word 迴圈。權限檢查跟 `PTRACE_ATTACH`/`mem` 是同一套，只是搬資料的路徑更精簡，是大量記憶體讀寫（如 coredump 工具）常用的路。

## 三、權限 —— 為什麼不能隨便碰別人的記憶體

**基本規則（跟 Yama 無關、一直都在）**：只能 ptrace **同 uid** 的 process，除非你有 `CAP_SYS_PTRACE`（實務上約等於 root）。

**但同 uid 還不夠——桌機一個使用者底下同時跑幾十個 process（瀏覽器、ssh-agent、gpg-agent、各種背景服務），互相全同 uid。** 若同 uid 就能隨便互 ptrace，任何一個被騙裝進來的惡意程式都能附掛到瀏覽器、把記憶體裡的 session token、解密後的私鑰整段撈走，或者更狠——竄改暫存器＋寫入指令＝直接劫持另一支正在跑的程式的控制流。這正是多數 debugger/cheat engine/也是不少 exploit 的共同底層招式。

於是 Linux 加了一層 **Yama LSM 的 `ptrace_scope`**，控制檔 `/proc/sys/kernel/yama/ptrace_scope`：

| 值 | 行為 |
|---|---|
| `0`（classic） | 回到最原始規則：同 uid 就能互相 ptrace（跨 uid 仍要 `CAP_SYS_PTRACE`） |
| `1`（restricted，多數發行版預設） | 只能 ptrace **自己的直系子孫 process**；要 ptrace 非子孫，對方得先呼叫 `prctl(PR_SET_PTRACER, 你的pid)` 明確授權（當機回報工具如 apport 靠這個），否則要 `CAP_SYS_PTRACE` |
| `2`（admin-only） | 只有 `CAP_SYS_PTRACE`（root 等級）能用 ptrace，一般使用者完全不行 |
| `3`（no attach） | 整台機器關掉 ptrace，連 root 都不行，重開機才解除 |

這解釋了一個常見疑惑：**為什麼 `gdb -p <pid>` 附掛一個不相干的 process 常常失敗、要 `sudo`**——scope=1 下你不是它的祖先，就是不給附掛，這是刻意的預設安全邊界，不是 bug。反過來，`gdb ./a.out` 自己跑起來再中斷點沒問題，因為 debugger 是 tracee 的**直接父 process**。

## 四、接 unipath / Plan 9 —— 「執行態可路徑定址」的最深形式

前幾篇一路在談「把**自己**這個 process 的狀態、fd、記憶體映射暴露成路徑/檔案」——`unipath_live.py`（step 2）暴露的就是**自己 process 的活物件圖**：`echo > .../data` write-through 改到活的 Python 物件，讀到的值隨背景 thread 變動，是「執行態非快照」。

**ptrace + `/proc/<pid>/mem` 把同一個故事推到極致**：不是暴露「自己」，而是暴露並操縱**別人**——連另一個 process 的暫存器、下一條要跑的指令、整片記憶體，都能透過（幾乎是）檔案介面的 `open`/`lseek`/`read`/`write` 碰到。這是「執行態可路徑定址」在 Linux 上能走到的最深處：不只是「這個活著的東西有個路徑」，而是「這個活著的東西的**每一個位元組**都可以被外部 walk 到、讀寫到」。

**Plan 9 把這件事做得更乾淨、更貫徹一開始就講的哲學**（見 [背景-檔案動詞與process與mmap.md](背景-檔案動詞與process與mmap.md) §2.1）：
- Plan 9 一樣有 `/proc/<pid>/mem`，語意相同——位址空間即檔案。
- 但**沒有** `ptrace` 這種帶外、多用途、參數靠魔數區分的 syscall 大雜燴；Plan 9 一貫做法是**寫 `ctl` 檔**：寫入字串 `stop`/`start`/`kill` 就控制執行、寫 `note` 送信號。跟 [背景-fd操作三組.md](背景-fd操作三組.md) 講的「Plan 9 廢掉 ioctl、改寫 ctl 檔」是同一招在 process 控制上的翻版——**Unix 把「暫停/控制」塞進 ptrace 這個獨立 syscall；Plan 9 把它化約成「寫一個檔案」**。

**接回 unipath 現況與可能的下一步**：unipath step 2/3 暴露的是「自己這支 Python process 的物件圖」；ptrace/`mem` 展示的是「**同一套 read/write-over-path 語意，理論上可以延伸到暴露並操縱別人的 process**」——如果 unipath 有一天想做「掛一個除錯/檢視某支跑著的程式」的元素類型，這裡就是要抄的底層機制：attach 拿到權限、`maps` 找位址、`mem` 讀寫、`ctl` 檔轉譯成 `PTRACE_CONT`/`PTRACE_SYSCALL`/送信號這類動作。權限那道牆（同 uid / `CAP_SYS_PTRACE` / `ptrace_scope`）也提醒了一件事：**unipath 的檔案樹只要接上真正跨 process 的操縱能力，就同時接上了它的攻擊面**——這正是 Plan 9／Unix 都要為它另立一套權限檢查的原因，不能只靠 9P 本身的 uname/權限位。
