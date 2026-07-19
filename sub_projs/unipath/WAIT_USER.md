# WAIT_USER — 待使用者親自做 / 驗證（只列 open）

← [INDEX](INDEX.md)。完成即移除。

## Open

- **核心 v9fs 真掛載驗證**（需 root，我無法替你 sudo）——驗 step 4 的真 9P 能被 Linux 核心直接掛：
  ```
  # 終端 A（免 root）
  cd sub_projs/unipath && .venv/bin/python unipath_9p.py serve 5640
  # 終端 B
  sudo modprobe 9pnet_fd 9p
  mkdir -p /tmp/umnt
  sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000,uname=$USER 127.0.0.1 /tmp/umnt
  ls /tmp/umnt          # 期望：0 1 2 data ctl status
  cat /tmp/umnt/0/data  # 期望：一個整數（隔幾秒再讀會變，因背景 tick）
  sudo umount /tmp/umnt
  ```
  讀一定通；透過核心 mount **寫入**涉及 9P uid 權限，要調再說。
