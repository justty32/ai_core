# Linux 專屬：inode flags / 安全上下文 / file capabilities

← [調查總覽](README.md)

這三種是 Linux（非 POSIX 通用）的中繼資料，設定各有專用工具與庫。

## 1. inode flags（immutable、append-only… 由 chattr 管）

ext*／xfs／btrfs 的 inode 有一組**行為旗標**，最有名的是 `immutable`（連 root 也不能改/刪，除非先卸旗）與 `append-only`（只能追加）。

**CLI**（`chattr`／`lsattr`，e2fsprogs）：
```bash
chattr +i f       # immutable：不能改內容/改名/刪/連結（需 root）
chattr +a f       # append-only：只能 O_APPEND 寫入（需 root）
chattr -i f       # 卸掉
chattr +A f       # 不更新 atime（省 IO）
chattr +d f       # dump 時跳過；+c 壓縮（視 fs）；+C 關 COW（btrfs/xfs）
lsattr f          # 讀（顯示如 ----i---------e----）
```

**C**（ioctl，`<linux/fs.h>` + `<sys/ioctl.h>`）：
```c
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>    // FS_IOC_GETFLAGS/SETFLAGS, FS_IMMUTABLE_FL, FS_APPEND_FL, FS_NOATIME_FL...
int fd = open("f", O_RDONLY);
long flags;
ioctl(fd, FS_IOC_GETFLAGS, &flags);   // 先讀現況（別覆蓋掉別的旗標）
flags |= FS_IMMUTABLE_FL;
ioctl(fd, FS_IOC_SETFLAGS, &flags);   // 寫回；immutable/append 需 CAP_LINUX_IMMUTABLE
close(fd);
```
- 旗標常數在 `<linux/fs.h>`：`FS_IMMUTABLE_FL`／`FS_APPEND_FL`／`FS_NOATIME_FL`／`FS_NODUMP_FL`／`FS_SYNC_FL`／`FS_NOCOW_FL`… （本機 grep 到全套）。
- 較新 API：`FS_IOC_FSGETXATTR`／`FS_IOC_FSSETXATTR`＋`struct fsxattr`（`<linux/fs.h>`），管 project id／DAX／COW 等擴充屬性；基本旗標用上面的經典 ioctl 即可。
- ⚠ 權限：`immutable`／`append-only` 需 `CAP_LINUX_IMMUTABLE`（≈root）——本機實測非 root `chattr +i` 直接 `Operation not permitted`。其餘如 `noatime` 檔主可設。
- ⚠ **可攜性**：純 Linux 且**與 fs 綁定**（ext4/xfs/btrfs 支援集不同）；FAT/exFAT 幾乎沒有。

## 2. SELinux 安全上下文（本機無 SELinux；通則）

在 **SELinux 系統**上，每個檔案有安全上下文 `user:role:type:level`（存在 `security.selinux` xattr），決定 MAC 存取。

**CLI**：
```bash
ls -Z f                                        # 看上下文
chcon -t httpd_sys_content_t f                 # 改 type（臨時，relabel 會被沖掉）
chcon --reference=ref f                         # 抄 ref 的上下文
restorecon -v f                                # 依 policy 還原成「正確」上下文（正規做法）
semanage fcontext -a -t T '/path(/.*)?'; restorecon -R  # 改 policy 的預設對應（持久）
```

**C**（libselinux，`<selinux/selinux.h>`，連結 `-lselinux`）：
```c
#include <selinux/selinux.h>
setfilecon("f", "system_u:object_r:httpd_sys_content_t:s0");   // 路徑
lsetfilecon("link", ctx);                                       // 不跟隨 symlink
fsetfilecon(fd, ctx);                                           // fd
```
- ⚠ 權限：`CAP_MAC_ADMIN` + policy 允許。
- ⚠ **本機（Manjaro）不裝 SELinux**（用 AppArmor）——`selinux/selinux.h`／`chcon` 皆不存在，本段未在本機驗；AppArmor 走 profile（不掛在單檔 xattr 上），不是「設檔案中繼資料」的模型，故不列入本調查的檔案級設定。

## 3. file capabilities（給執行檔綁最小特權，替代 setuid）

讓某執行檔啟動時帶特定 capability（如只綁低 port），比 setuid-root 安全。存在 `security.capability` xattr。

**CLI**（libcap 的 `setcap`／`getcap`）：
```bash
setcap 'cap_net_bind_service=+ep' ./server   # 允許綁 <1024 port，免 root（需 CAP_SETFCAP 設定）
getcap ./server                              # 讀
setcap -r ./server                           # 移除
```

**C**（libcap，`<sys/capability.h>`，連結 `-lcap`）：
```c
#include <sys/capability.h>
cap_t caps = cap_from_text("cap_net_bind_service+ep");
cap_set_file("./server", caps);              // 寫進檔案（需 CAP_SETFCAP）
cap_free(caps);
```
- ⚠ 權限：設定需 `CAP_SETFCAP`（≈root）。
- ⚠ 限制：只對**普通執行檔**有意義；跨 fs copy/tar 預設不帶（要 `--xattrs` / `cp --preserve=xattr`）。

## 保存/搬移時別掉中繼資料（連帶結論）

設好的中繼資料在 copy/打包時**預設可能遺失**：
- `cp` 要 `-p` 或 `--preserve=mode,ownership,timestamps,xattr,links,all`。
- `tar` 要 `--xattrs --acls --selinux`（GNU tar）才連 xattr/ACL/context 一起打包。
- `rsync` 要 `-X`（xattr）`-A`（ACL）`-p -o -g -t`（權限/owner/group/time）。
- 跨檔案系統（如落到 FAT/網路掛載）許多屬性會直接被丟棄——設計「以檔案中繼資料承載狀態」時要考慮這條可攜性懸崖。
