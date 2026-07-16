# 調查：Linux 檔案中繼資料怎麼設（C API × CLI）

← [investigation](../README.md)｜[INDEX](../../../INDEX.md)

調查日期 2026-07-16，於 Manjaro（kernel 6.18、ext4/tmpfs）實測 grounding。**主題**：一個檔案身上能掛哪些中繼資料、各自**怎麼設**（命令列工具 + C 語言 API）、需要什麼權限、可攜性如何。

> **為何在 ai_core**：北極星把「一切皆檔案／統一描述子」當組織原則（見 [notes 20260708 一切皆檔案](../../notes/20260708-1040-一切皆檔案-list-統一描述子-複合路徑.md)）。函式當檔案、要掛「自我描述」的中繼資料時，**xattr／inode flags** 是 OS 原生的掛載點——這份調查摸清底層機制，供該設計取用。

## 速查總表（設定面）

| 中繼資料 | CLI 設定 | C API 設定 | 頭檔／連結 | 權限 | 可攜性 |
|---------|---------|-----------|-----------|------|--------|
| 權限位 mode（rwx/suid/sgid/sticky）| `chmod` | `chmod` / `fchmod` / `fchmodat` | `<sys/stat.h>` | 檔主或 root | POSIX |
| 擁有者 uid / 群組 gid | `chown` / `chgrp` | `chown` / `fchown` / `lchown` / `fchownat` | `<unistd.h>` | 改 owner＝root；改 group＝檔主且屬該群 | POSIX |
| 時間戳 atime / mtime | `touch` | `utimensat` / `futimens` | `<sys/stat.h>` | 檔主或有寫權 | POSIX |
| ctime（狀態變更時間）| **不可直接設** | — | — | 任何 metadata 改動時自動更新 | — |
| btime（建立時間）| **不可設，可讀** | 讀＝`statx`；設＝無標準 API | `<sys/stat.h>` | — | Linux 4.11+ |
| 延伸屬性 xattr | `setfattr` / `getfattr` | `setxattr` / `fsetxattr` / `lsetxattr` | `<sys/xattr.h>` | `user.*`＝檔主；`trusted.*`／`security.*`＝CAP | Linux（+ *BSD/macOS 語意近似）|
| POSIX ACL | `setfacl` / `getfacl` | `acl_set_file` / `acl_set_fd`（libacl）| `<sys/acl.h>` `-lacl` | 檔主 | POSIX.1e draft；fs 需掛 `acl` |
| inode flags（immutable/append…）| `chattr` / `lsattr` | `ioctl` `FS_IOC_SETFLAGS`（或 `FS_IOC_FSSETXATTR`）| `<linux/fs.h>` | immutable/append 需 `CAP_LINUX_IMMUTABLE`（≈root）| Linux／ext*・xfs・btrfs |
| SELinux 安全上下文 | `chcon` / `restorecon` | `setfilecon` / `lsetfilecon`（libselinux）| `<selinux/selinux.h>` `-lselinux` | `CAP_MAC_ADMIN` | 僅 SELinux 系統 |
| file capabilities | `setcap` / `getcap` | `cap_set_file`（libcap）| `<sys/capability.h>` `-lcap` | `CAP_SETFCAP` | Linux |

> ★ **關鍵洞察**：ACL、SELinux 上下文、file capabilities **底層都是 xattr**（分別存在 `system.posix_acl_access`／`security.selinux`／`security.capability`）——`setfattr` 能看到它們的原始形式，但**該用專用工具/庫設**（有語意編碼，直接寫 raw xattr 易錯）。

## 細節分三檔

| 檔 | 內容 |
|----|------|
| [posix-core.md](posix-core.md) | **標準 POSIX**：權限 mode、擁有者 uid/gid、時間戳（含 ctime/btime 為何不可設）——CLI + C 範例 |
| [xattr-acl.md](xattr-acl.md) | **延伸屬性 xattr** 與 **POSIX ACL**——命名空間、CLI + C（含 libacl）|
| [linux-flags-security.md](linux-flags-security.md) | **Linux 專屬**：inode flags（chattr／`FS_IOC_SETFLAGS`）、SELinux 上下文、file capabilities |

## 實測 grounding（本機 2026-07-16）

- `touch -d '2020-01-02 03:04:05' f`＋`stat -c %y`：mtime 設定成功。`stat -c %w` 讀得到 btime（ext4 有記），但**無 API 可設 btime**。
- `setfattr -n user.comment -v x f`＋`getfattr -d f`：`user.*` xattr 非 root 可設成功。
- `setfacl -m u:USER:rwx f`＋`getfacl f`：ACL 設定成功（fs 掛了 `acl`）。
- `chattr +i f` 以非 root 跑 → `Operation not permitted`：證實 immutable 需 `CAP_LINUX_IMMUTABLE`。
- **本機無 SELinux**（Manjaro 用 AppArmor；`selinux/selinux.h`／`chcon` 皆不存在）——SELinux 段以「在 SELinux 系統上」的通則寫，未在本機驗。其餘頭檔（`sys/xattr.h`／`sys/acl.h`／`linux/fs.h`／`sys/capability.h`）本機皆在。

> **通用注意**：多數「設定」路徑對符號連結有 `l*` 變體（`lsetxattr`／`lchown`…）＝作用在 link 本身不跟隨；`f*` 變體（`fsetxattr`／`fchmod`…）＝吃 fd 而非路徑；`*at` 變體（`fchmodat`／`utimensat`…）＝相對某 dirfd，避免 TOCTOU。
