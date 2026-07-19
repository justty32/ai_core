# WAIT_USER — 待使用者親自做 / 驗證（只列 open）

← [INDEX](INDEX.md)。完成即移除。

## Open

（目前無）

## 已驗（歷史；完成項通常移除，此條保留作里程碑錨點）

- ✅ **核心 v9fs 真掛載驗證**（2026-07-19，使用者親自跑）：`sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000` 掛 `unipath_9p.py` 成功；`ls` 見 `0 1 2 ctl data status`、`cat /0/data` 透過核心讀到活 process 的即時整數。step 4「可被核心 v9fs 掛載」empirically 坐實。
