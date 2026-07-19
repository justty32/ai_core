# unipath — 全景文（回家掌握用）

> 一頁看懂 unipath 是什麼、為什麼這樣做、做到哪、怎麼跑、下一步。
> 視覺速覽版見同目錄 [`overview.html`](overview.html)（瀏覽器開）。頂層路由見 [AGENTS.md](AGENTS.md)。

---

## 一句話

**unipath ＝「先歸一於路徑、後成局」願景的階段一落地**——採 **Plan 9 思想＋9P 協議**、以 **FUSE 起步**，把「一個正在跑的 process 的執行態環境」暴露成一棵**可 walk 的路徑樹**，外部用 `ls`/`cat`/`echo` 就能定址並讀寫其中元素。**Python 原型優先、暫不考慮效能。**

## 為什麼（設計由來）

願景與決策全在 ai_core 筆記鏈，動手前的完整脈絡在那裡：

1. **世界（OS-as-game）篇** — OS 當回合制世界、tick、NPC、檔案系統即場景。
2. **兩層底座重解篇** — 有序兩階段：先「歸一於路徑」（一切可路徑定址：靜態物／執行態／呼叫方式），後「成局」。時間靠 `tick` 外掛。
3. **實作路線規劃篇（本專案的由來）** — 收斂出 **方案 A**：採 Plan 9 思想＋9P，蓋在 Linux（FUSE/9P），不去 boot Plan 9（那條留作逃生門）。

> 檔案位置：`../../workflows/notes/20260719-*.md`。

## 核心概念（五個要點）

1. **歸一於路徑**：一切都可用路徑定址——不只靜態檔案，連**正在執行的東西**與**呼叫方式**都收斂成路徑。
2. **9P 是那個通用協議**：Plan 9 早把「一切皆檔案」貫徹到底；9P 是它的線協議。規範層從「你得是某語言」變成「**你得會講 9P**」——天生語言中立。
3. **Plan 9 慣例：`ctl`/`data`/`status`**：每個「元素」是一個目錄，含三個約定檔——`data`（讀寫內容）、`ctl`（寫控制命令，取代 ioctl）、`status`（讀狀態）。
4. **兩種索引都自然落成路徑**：`list`/tuple → 數字子路徑（`/0` `/1`，0-based）；`dict` → 字串鍵子路徑（`/name` `/hp`）。
5. **執行態非快照**：讀到的是**此刻**的狀態；發布端背景在 tick，同一路徑不同時刻讀到不同值；`echo` 寫下去改的是**活物件本身**。

## 做到哪：五刀進度（全綠、全 commit）

| 刀 | 是什麼 | 關鍵驗證 | 入口（核心模組見 [INDEX](INDEX.md)）|
|---|---|---|---|
| **step 1** | 假靜態樹，證 FUSE mount 管線 | `ls`/`cat`/`echo` walk/讀/寫皆通 | `unipath_fs.py`（`up_static`/`up_fuse`）|
| **step 2** | 暴露**自身** process 執行態 | counter 隨時間變（非快照）、`echo` write-through、`ctl` | `unipath_live.py`（`up_env`/`up_fuse`）|
| **step 3** | **跨 process** 暴露 | 從外部 walk 另一 process 的 env、獨立第三方確認改到發布端本身 | `unipath_pub.py` + `unipath_mount.py` |
| **step 4** | **真 9P2000 線格式** | 獨立真 9P client 自測互通；**✅ 已被核心 `v9fs` 真掛載驗證**（2026-07-19，`ls`/`cat` 透過核心讀活值）| `unipath_9p.py`（`up_ninep*`）|
| **step 5** | **tick 回合制狀態轉移** | counter 累加、echo 抄鄰居、guard 傷害/回血/死亡、使用者影響於回合邊界吸收 | `unipath_tick.py`（`up_tick`/`up_tick_rules`）|
| **step 6** | **腳本化 NPC＋巢狀 tick＋影響走路徑樹**（階段二種子）| NPC 行為＝住 `/idx/script/data` 的腳本、`echo` 改行為即生效；巢狀 town 每父回合跑 rate 拍；影響＝寫樹 | `unipath_world.py`（`up_tick_script`）|

**里程碑**：step 3 做完「歸一於路徑第二類」；step 4 坐實「規範＝講 9P」＋跨機器兩世界通聯；step 5 把時間軸落地（`tick(狀態, 影響) → 下個狀態`）；**step 6 讓「規則本身也住在路徑樹裡、可 `echo` 編輯」——NPC＝可定址的腳本，正式踏進階段二 os-as-game。**

## 多語言 port：語言中立實證

規範層宗旨是「**你得會講 9P**，跟語言無關」。已用三種語言各寫一版 9P server，並用**同一個 Python 9P client** 逐位元組對話驗證互通：

| port | 怎麼實作 | 互通驗收 |
|---|---|---|
| **Python**（`up_ninep*`） | 參考實作 | ✅ SELFTEST OK |
| **C++**（[ports/cpp](ports/cpp/README.md)） | raw socket、g++、無 libfuse | ✅ SELFTEST OK（逐位元組近乎全同）|
| **Fennel**（[ports/fennel](ports/fennel/README.md)） | Lisp-on-Lua、LuaJIT FFI 手綁 socket | ✅ SELFTEST OK |

> 同一 client 能跟 Python / C++ / Fennel 三種 server 對話 ⇒ 「語言中立＝講 9P」落地。各 port 每檔亦 <100 行。

## 架構

```
step 2（自掛）:   [Python process | 活 env + 背景 tick] ── FUSE ──> mnt/
                   同一個 process 既持有 env 又當檔案系統

step 3（跨 process）:
   [發布端 process]                       [掛載端 process]
   活 env + tick                          薄 FUSE 客戶端（無 world 狀態）
   監聽 Unix socket  <── 9P 形狀 JSON-RPC ──  每個 FS 操作轉一次 RPC
                                              └─ FUSE ──> mnt/

step 4（真 9P）:
   [發布端 process]                       [核心 v9fs]（或獨立 9P client / C++ / Fennel server）
   活 env + tick                          mount -t 9p
   監聽 TCP:5640  <──── 真 9P2000 線格式 ────  ls/cat/echo（走核心，脫離 FUSE）

step 5（tick）:
   TickWorld（活 env，關背景 +1）── run(N 回合) ──> 每回合：吸收使用者影響 → 拍全樹快照 → 依規則同步更新
   這棵 world 可被 step 3/4 發布端拿去暴露，外部即看到回合制轉移
```

- `up_env.py`（`Env`）＝執行態環境的**純邏輯**（無 FUSE/socket 依賴），被 FUSE(`up_fuse`) 與 9P(`up_ninep`) 共用。
- 一路「介面照 9P 慣例擺」，故 step 3→4 是平滑升級（JSON 形狀 → 真線格式）。
- 程式碼已正規化拆檔（**每檔 <100 行**：共用邏輯抽 `up_*`、step 腳本退成薄入口）——完整地圖見 [INDEX.md](INDEX.md)。

## 怎麼跑

前置：`.venv` 已建、裝好 `fusepy`。指令都在 `sub_projs/unipath/` 下。

**step 2（自掛，最簡）**
```bash
.venv/bin/python unipath_live.py mnt        # 前景掛載
# 另個終端：
ls mnt ; cat mnt/0/data ; cat mnt/0/data    # 兩次不同 → 執行態
echo 999 > mnt/2/hp/data ; cat mnt/2/data   # write-through
fusermount -u mnt                           # 卸載
```

**step 3（跨 process）**
```bash
.venv/bin/python unipath_pub.py /tmp/unipath.sock          # 終端 A：發布端
.venv/bin/python unipath_mount.py /tmp/unipath.sock mnt    # 終端 B：掛載端
ls mnt ; cat mnt/0/data ; echo 777 > mnt/2/hp/data         # 終端 C
fusermount -u mnt                                          # 卸載
```

**step 4（真 9P）**
```bash
.venv/bin/python unipath_9p.py serve 5640      # 發布端（免 root）
.venv/bin/python unipath_9p.py selftest 5640   # 獨立真 9P client 自測
```
核心 v9fs 掛載（**需 root，自己跑**）：
```bash
sudo modprobe 9pnet_fd 9p
mkdir -p /tmp/umnt
sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000,uname=$USER 127.0.0.1 /tmp/umnt
ls /tmp/umnt ; cat /tmp/umnt/0/data   # 隔幾秒再讀 → 數字在變
sudo umount /tmp/umnt
```

**step 5（tick）**
```bash
.venv/bin/python unipath_tick.py               # 印出連續幾回合，證狀態轉移＋影響被吸收
```

**多語言 port（跨語言互通）** — 啟任一語言 server，用 Python client 打它：
```bash
( cd ports/cpp && make && ./unipath_9p_cpp serve 5661 )      # C++
( cd ports/fennel && ./serve.sh 5662 )                       # Fennel
.venv/bin/python unipath_9p.py selftest 5661   # → SELFTEST OK ＝ 跨語言互通
```

## 界線與代價（誠實標註）

- **step 3 是 9P 的「形狀」**（JSON-over-socket），**step 4 才是真 9P 線格式**。
- **真 9P 掛載需 root**（mount 天生特權；FUSE 才免特權）。讀一定通；透過核心 mount **寫入**涉及 9P uid 權限，要用再調。
- **忽略 CPU 週期**：這套慢循環暫忽略效能（僅限「慢世界」；筆記裡的「快世界」那層不在此列）。
- **Python 只是參考實作**：規範層才是重點（＝講 9P，語言中立）。

## 怎麼跑（step 6）

```bash
.venv/bin/python unipath_world.py   # 腳本化 NPC＋巢狀 tick＋影響走路徑樹
```

## 下一步（roadmap，往階段二；詳見 [SESSION-LOG](SESSION-LOG.md)）

- **script 沙箱強化**：目前 `exec`＋白名單夠試驗田；收外來 world 要更嚴（受限 DSL / Lisp 表達式）。
- **世界持久化**：world 存成真檔案樹（非記憶體物件），tick 讀寫磁碟 → 呼應「檔案分佈＝空間狀態」。
- **多 NPC 互動 / LOD**：元素依 `peer` 互相影響、略圖規則（承世界篇 §十三）。
- 其他：真 9P 寫入的 uid/權限、9P2000.L 擴充、ports 的 ctl 對齊。

## 專案結構（正規化後）

已套 `~/repo/workflows` 的 dev flavor 分層脊椎：[AGENTS](AGENTS.md)（薄路由）→ [INDEX](INDEX.md)（檔案地圖）/ [WORKFLOWS](WORKFLOWS.md)（意圖派發）→ 程式碼 / [docs/](docs/)（8 篇作業系統背景＋現況清單）/ [ports/](ports/)。活狀態：[SESSION-LOG](SESSION-LOG.md)、[WAIT_USER](WAIT_USER.md)。結構規則：[DEV-GUIDE](DEV-GUIDE.md)（含 <100 行硬規）。
