# 標準 POSIX 中繼資料：權限 / 擁有者 / 時間戳

← [調查總覽](README.md)

`stat(2)` 結構裡那批「每個 unix 檔案都有」的中繼資料，設定路徑最標準、最可攜。

## 1. 權限位 mode（rwx＋suid/sgid/sticky）

**CLI**：
```bash
chmod 644 f            # 八進位：rw-r--r--
chmod u+x,g-w f        # 符號式
chmod 4755 f           # setuid（前導 4）；2xxx=setgid；1xxx=sticky
chmod -R u+rwX dir/    # 遞迴；大寫 X＝只對目錄或已可執行者加 x
umask 022              # 影響「之後新建」檔案的預設 mode，不改既有檔
```

**C**（`<sys/stat.h>`）：
```c
#include <sys/stat.h>
chmod("f", 0644);                         // 路徑
fchmod(fd, 0644);                          // 開好的 fd
fchmodat(AT_FDCWD, "f", 04755, 0);         // *at 版：相對 dirfd，避 TOCTOU
mode_t old = umask(022);                   // 設並回舊值
```
- mode 常數：`S_IRUSR|S_IWUSR`（0600）、`S_ISUID`（04000）、`S_ISGID`（02000）、`S_ISVTX`（sticky, 01000）。
- 實際落地 mode ＝ 你給的 mode `& ~umask`（僅新建時）；`chmod` 直接設、不吃 umask。

## 2. 擁有者 uid / 群組 gid

**CLI**：
```bash
chown alice f          # 只改 owner（需 root）
chown alice:staff f    # owner + group
chgrp staff f          # 只改 group（檔主且屬該群即可，免 root）
chown -R alice dir/    # 遞迴
chown --reference=a b  # 抄 a 的 owner/group 到 b
```

**C**（`<unistd.h>`）：
```c
#include <unistd.h>
chown("f", uid, gid);                 // gid/uid 傳 (uid_t)-1 表示「不動這欄」
fchown(fd, uid, (gid_t)-1);
lchown("link", uid, gid);             // 作用在 symlink 本身、不跟隨
fchownat(AT_FDCWD, "f", uid, gid, AT_SYMLINK_NOFOLLOW);
```
- ⚠ **權限規則**：改 owner 幾乎一定要 root（`CAP_CHOWN`）；改 group 只需「你是檔主且是目標 group 的成員」。
- ⚠ **副作用**：`chown` 成功會**清掉 setuid/setgid 位**（安全考量），改完 owner 若還要 suid 得重設 mode。

## 3. 時間戳 atime / mtime（可設）

一個 inode 有三個時間，Linux 另有第四個：
- **atime**（access，最後讀取）、**mtime**（modification，內容最後改）— **可設**。
- **ctime**（change，inode 狀態最後變）— **不可直接設**（任何 metadata 改動時 kernel 自動更新）。
- **btime/crtime**（birth，建立）— **不可設**，Linux 4.11+ 可用 `statx` 讀（見下）。

**CLI**（`touch`）：
```bash
touch f                              # atime+mtime 設成「現在」（不存在則建空檔）
touch -c f                           # 不存在就不建
touch -m -d '2020-01-02 03:04:05' f  # 只改 mtime，指定時間（-a 只改 atime）
touch -t 202001020304.05 f           # [[CC]YY]MMDDhhmm[.ss] 格式
touch -r ref f                       # 抄 ref 的時間戳到 f
```

**C**（`<sys/stat.h>`，`utimensat`／`futimens`，納秒精度）：
```c
#include <sys/stat.h>   // struct timespec, UTIME_NOW, UTIME_OMIT
struct timespec ts[2];  // [0]=atime, [1]=mtime
ts[0].tv_sec = 1577905445; ts[0].tv_nsec = 0;   // 指定 atime
ts[1].tv_nsec = UTIME_NOW;                        // mtime 設現在（tv_sec 被忽略）
// ts[k].tv_nsec = UTIME_OMIT;  // 該欄不動
utimensat(AT_FDCWD, "f", ts, 0);                  // 路徑；flag 可 AT_SYMLINK_NOFOLLOW
futimens(fd, ts);                                 // fd
utimensat(AT_FDCWD, "f", NULL, 0);                // ts=NULL → 兩者都設成現在（等同 touch f）
```
- 舊 API（仍可用，秒/微秒精度）：`utimes(2)`（`struct timeval[2]`）、`utime(2)`（`<utime.h>`，`struct utimbuf{actime,modtime}`）。新碼一律用 `utimensat`。
- ⚠ 權限：設成「現在」只需寫權；設成**任意時間**要求你是檔主（或 `CAP_FOWNER`）。

## 4. ctime / btime：為何不能設

- **ctime**：設計上就是「防竄改的旁證」——你動 mode/owner/內容/xattr，ctime 一定跟著跳。**沒有 API 設它**（root 也不行，除非改系統時鐘再 touch，屬歪招）。
- **btime**：多數 fs（ext4/xfs/btrfs）在 inode 有記，但 kernel **只提供讀不提供寫**。讀法：
```c
#include <sys/stat.h>   // statx, STATX_BTIME
struct statx sx;
statx(AT_FDCWD, "f", 0, STATX_BTIME, &sx);
if (sx.stx_mask & STATX_BTIME)
    /* sx.stx_btime.tv_sec / tv_nsec 有效 */;
```
  CLI 對應：`stat -c %w f`（`-` 代表 fs 沒記或不可讀）。要「改」btime 只能 debugfs 之類離線硬改，非正規路徑。

> 小結：這層四種裡，**mode／owner／atime／mtime 可設**（上面三組 API），**ctime／btime 不可設**。延伸屬性與 Linux 專屬旗標見另兩檔。
