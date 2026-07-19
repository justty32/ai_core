# unipath — 歸一於路徑的落地基材（方案 A 實作）

unipath = **「先歸一於路徑，後成局」願景的階段一落地**——採 **Plan 9 思想＋9P 協議**、以 **FUSE 起步**，把「一個活 process 的執行態環境」暴露成一棵**可 walk 的路徑樹**，外部用 `ls`/`cat`/`echo` 就能定址並讀寫其中元素。**Python 原型優先、暫不考慮效能。**

> 暫名，可改。本檔是**最頂層入口**：只指向設計真源與當前狀態，durable 細節不堆這裡。

## 定位（與 handy 的分工）

- [handy](../handy/AGENTS.md) ＝「**拿現成程式用腳本包裝**、資料夾＝callable」——較上層。
- **unipath ＝更底層**：不是包裝現成程式，而是**把 process 環境本身暴露成路徑樹**（`/proc` 式、Plan 9 式）。所以另起乾淨地、不塞進 handy。

## 設計真源（唯一，先讀）

願景與決策全在 ai_core 的筆記鏈，**動手前先讀**：

1. [世界（OS-as-game）篇](../../workflows/notes/20260719-1150-世界-os-as-game-tick-npc-檔案系統即場景.md)
2. [兩層底座重解：歸一於路徑＋tick](../../workflows/notes/20260719-1301-兩層底座重解-歸一於路徑-tick即時間轉換.md)
3. [**實作路線規劃（本專案的由來）**](../../workflows/notes/20260719-1518-實作路線規劃-採plan9與9p蓋在linux-方案a.md) ← 方案 A、FUSE、Python 等定調都在此

## 已定調（來自路線篇）

- **規範層＝「講 9P」**（語言中立）；真 Plan 9 留作「以後有需要再換」的逃生門。
- **實作＝FUSE 蓋在 Linux**（`fusepy`/`pyfuse3` 類），介面**照 9P 慣例擺**，日後平滑轉真 9P。
- **控制/資料模型抄 Plan 9 慣例**：`data` 檔讀寫 ＋ `ctl` 檔寫控制命令 ＋ `status` 讀狀態（取代 ioctl）。
- **原型載體＝Python**，原型優先、暫不管效能（合「Python 只是參考實作」宗旨）。

## 當前狀態

- **Step 1 完成 ✅**（假靜態樹證 mount 通路）：`unipath_fs.py`（Python＋fusepy）掛一棵 0-based 數字樹，
  每個元素目錄含 `data`/`ctl`/`status` 約定檔。已驗：`ls` walk（含巢狀）、`cat` 讀、`echo >` 寫 data、
  `ctl` 命令（`set`/`mkelem`/`rmelem`）皆通。跑法：`.venv/bin/python unipath_fs.py mnt`（前景），另開終端 `ls/cat/echo mnt/…`，`fusermount -u mnt` 卸載。
- **Step 2 完成 ✅**（真執行態環境）：`unipath_live.py` 把一個活 Python process 的物件圖暴露成路徑樹——
  list→數字子路徑、dict→字串鍵子路徑；背景 thread 持續改 `world[0]`，故同路徑不同時刻讀到不同值（＝執行態非快照）；
  `echo > …/data` write-through 改活物件；`ctl` 支援 `append`/`set`/`del`。跑法同 step 1（換 `unipath_live.py`）。
- **Step 3 完成 ✅**（跨 process 暴露）：拆成 `unipath_env.py`（純邏輯）＋`unipath_pub.py`（發布端 process，持有活 env＋監聽 Unix socket）＋`unipath_mount.py`（薄 FUSE 客戶端，每操作轉 **9P 形狀 RPC**）。
  已驗：從外部 walk 另一 process 的 env、跨 process 執行態（tick 可見）、跨 process write-through（獨立第三方確認改到發布端本身）、ctl。
  跑法：終端 A `unipath_pub.py /tmp/unipath.sock`；終端 B `unipath_mount.py /tmp/unipath.sock mnt`；終端 C `ls/cat/echo mnt/…`。
  **界線**：step 3 是 9P 的**形狀**（JSON-over-socket）；真線格式見 step 4。
- **Step 4 完成 ✅**（真 9P2000 線格式）：`unipath_9p.py` 以逐位元組真 9P2000 服務 Env 樹（Tversion/Tattach/Twalk/Topen/Tread〔含目錄 stat 項〕/Twrite/Tclunk/Tstat）。
  已用**獨立真 9P client** 自測互通：`./unipath_9p.py serve 5640` ＋ `./unipath_9p.py selftest 5640`（讀活 counter、讀目錄、write-through 皆通）。
  **核心 v9fs 掛載（需 root，使用者自跑）**：`sudo modprobe 9pnet_fd 9p` 後
  `sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000,uname=me 127.0.0.1 <mnt>`。
- **Step 5 待議**：接 **tick** 狀態轉移語意／往階段二 os-as-game。
- 依賴：`fusepy`（純 ctypes 綁 libfuse2）；venv 在 `.venv/`（gitignored）。

## 鐵律

1. **試驗田心態**：低風險、隨時可推倒；先跑通再固化，別開工前立法。
2. **先 hack 出能跑的最小暴露，再回填規範**（有 9P 現成規範可抄，不從零）。
3. **未經確認不 push、不開新工作**。
4. **全繁體中文**留檔；識別子／指令／技術名詞保留原文。
