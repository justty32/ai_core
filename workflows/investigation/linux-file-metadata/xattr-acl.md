# 延伸屬性 xattr 與 POSIX ACL

← [調查總覽](README.md)

xattr 是「掛任意 key=value 在檔案上」的原生機制——**最適合掛自訂中繼資料**（如 ai_core 的函式自我描述）。ACL 是建在 xattr 之上的細粒度權限。

## 1. 延伸屬性 xattr（`name=value` 任意鍵值）

**命名空間**（`name` 的前綴決定語意與權限）：
| 前綴 | 用途 | 設定權限 |
|------|------|---------|
| `user.*` | **一般用途、自訂中繼資料** | 檔主即可（普通檔案；⚠ 目錄/symlink/特殊檔另有限制）|
| `trusted.*` | 特權程式用、一般使用者看不到 | 需 `CAP_SYS_ADMIN`（≈root）|
| `security.*` | LSM／capabilities（SELinux、capability 存這）| 由對應子系統管 |
| `system.*` | 核心用（POSIX ACL 存這：`system.posix_acl_access`）| 經專用 API |

**CLI**（`setfattr`／`getfattr`，attr 套件）：
```bash
setfattr -n user.comment -v "cllm function meta" f   # 設
setfattr -n user.blob -v 0sBASE64==  f               # 0s 前綴＝base64 值（存二進位）
getfattr -d f                                         # dump 所有 user.* （預設只列 user 命名空間）
getfattr -n user.comment f                            # 取單一
getfattr -m '' -d f                                   # -m '' ＝所有命名空間（含 security.*）
setfattr -x user.comment f                            # 移除
```

**C**（`<sys/xattr.h>`）：
```c
#include <sys/xattr.h>
// 設（flags: 0=建或換, XATTR_CREATE=只建不換, XATTR_REPLACE=只換不建）
setxattr("f", "user.comment", "hi", 2, 0);
fsetxattr(fd, "user.comment", "hi", 2, 0);
lsetxattr("link", "user.comment", "hi", 2, 0);   // 作用在 symlink 本身
// 讀：先問長度（value=NULL），再配 buffer 取
ssize_t n = getxattr("f", "user.comment", NULL, 0);
char buf[256]; getxattr("f", "user.comment", buf, sizeof buf);
// 列名（\0 分隔的名字清單）
char names[1024]; ssize_t ln = listxattr("f", names, sizeof names);
// 移除
removexattr("f", "user.comment");
```
- 每個變體都有 `f*`（fd）與 `l*`（不跟隨 symlink）版本，簽章對稱。
- ⚠ **可攜性/限制**：`user.*` 在**普通檔案**才寬鬆；對**目錄**要 sticky 或檔主、對 **symlink/device** 多半禁止。fs 需支援（ext4/xfs/btrfs 皆可；FAT/tmpfs 部分受限，本機 tmpfs 實測 `user.*` 可設）。值大小有上限（ext4 常見單值 ≤ 一個 block、總量受限）。
- ⚠ **別直接寫 `system.*`／`security.*` raw**：那是 ACL/SELinux/capability 的編碼，用專用工具（下）。

## 2. POSIX ACL（細粒度權限，超越 owner/group/other 三段）

ACL 讓你對**特定 user/group** 加權限，底層存在 `system.posix_acl_access`（目錄另有 `system.posix_acl_default` 決定子項繼承）。fs 需**掛載時帶 `acl`**（現代發行版多為預設）。

**CLI**（`setfacl`／`getfacl`，acl 套件）：
```bash
setfacl -m u:alice:rwx f          # 給 user alice rwx
setfacl -m g:staff:r-x f          # 給 group staff
setfacl -m m:rx f                 # 設 mask（有效權限上限）
setfacl -x u:alice f              # 移除某條
setfacl -b f                      # 清掉所有 ACL（回退純 mode）
setfacl -d -m u:alice:rwx dir/    # 目錄的 default ACL（子項繼承）
setfacl -R -m u:alice:rX dir/     # 遞迴
getfacl f                         # 讀（有 ACL 時 ls -l 的 mode 尾會顯示 '+'）
getfacl a | setfacl --set-file=- b  # 抄 a 的 ACL 到 b
```

**C**（libacl，`<sys/acl.h>`，連結 `-lacl`）：
```c
#include <sys/acl.h>
// 從文字建 ACL 再套上檔案（最省事）
acl_t acl = acl_from_text("u::rw-,u:alice:rwx,g::r--,m::rwx,o::r--");
acl_set_file("f", ACL_TYPE_ACCESS, acl);      // 目錄 default 用 ACL_TYPE_DEFAULT
acl_free(acl);
// 或程式化：acl_get_file → acl_create_entry → acl_set_tag_type/qualifier/permset → acl_set_file
acl_t cur = acl_get_file("f", ACL_TYPE_ACCESS);
// fd 版：acl_set_fd(fd, acl)（僅 ACCESS 型）
```
- 編譯：`cc x.c -lacl`。
- ⚠ 權限：改 ACL 要你是檔主（或 `CAP_FOWNER`）。設了 ACL 後，傳統 `chmod` 會改動 mask，兩者互動要留意（`getfacl` 看實際生效）。

> 這兩者都落在 xattr 上：`user.*` 是你自由發揮的中繼資料掛點；ACL 是 kernel 給 `system.posix_acl_*` 的語意包裝——**要細權限用 libacl/setfacl，別手寫 raw xattr**。Linux 專屬的 inode flags 與安全上下文見 [linux-flags-security.md](linux-flags-security.md)。
