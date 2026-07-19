# INDEX — unipath 專案地圖

unipath = 「先歸一於路徑、後成局」階段一落地（Plan 9 思想＋9P，FUSE 起步）。
[AGENTS.md](AGENTS.md) 只做路由；結構細節在此。全景見 [OVERVIEW.md](OVERVIEW.md)。

## 程式碼佈局（每檔 < 100 行；共用邏輯抽 `up_*`、step 腳本為薄入口）

**核心執行態環境**
| 檔 | 角色 |
|---|---|
| `up_world.py` | 常數 / `UnipathError` / 世界建構 / leaf 渲染 / 字面值解析 |
| `up_env.py` | `Env`：執行態環境（路徑導航＋stat/readdir/read/write＋ctl） |
| `unipath_env.py` | 相容轉出（`Env` / `UnipathError`，舊 import 續用） |

**FUSE（step 1–2）**
| `up_fuse.py` | FUSE 轉接器：任何具 stat/readdir/read/write 的 env → 檔案系統 |
| `up_static.py` | step 1 假靜態樹 env（Element＋ctl set/mkelem/rmelem） |

**9P（step 4）**
| `up_ninep_codec.py` | 9P 編解碼（p2/p4/ps/Reader）＋訊息常數 |
| `up_ninep.py` | `NineP` server：編碼（qid/stat）＋連線迴圈＋監聽 |
| `up_ninep_ops.py` | 9P 訊息分派 `dispatch()` |
| `up_ninep_client.py` | 獨立真 9P client（`selftest`；跨語言驗收也用它） |

**tick（step 5）**
| `up_tick_rules.py` | 世界建構＋每種 kind 規則＋使用者影響 |
| `up_tick.py` | `tick()` 轉移函數＋`TickWorld` 回合驅動器 |

**進階 tick / 階段二種子（step 6）**
| `up_tick_script.py` | 腳本化轉移引擎：NPC＝可定址 script、巢狀 tick、影響走路徑樹 |
| `unipath_world.py` | step 6 示範入口（腳本住樹可 echo 改行為 + 巢狀 town + 影響寫樹） |

**step 薄入口**
| `unipath_fs.py` | step 1 假樹（FUSE） · `unipath_live.py` step 2 執行態（FUSE） |
| `unipath_pub.py`＋`unipath_mount.py` | step 3 跨 process（9P 形狀 RPC） |
| `unipath_9p.py` | step 4 真 9P（`serve` / `selftest`） · `unipath_tick.py` step 5 示範 |

## 進度（全綠，全 commit）

step 1 假樹 → step 2 自身執行態 → step 3 跨 process → step 4 真 9P2000 → step 5 tick 回合制 → **step 6 腳本化 NPC＋巢狀 tick＋影響走路徑樹（階段二種子）**。詳見 [OVERVIEW.md](OVERVIEW.md)。open 項見 [SESSION-LOG](SESSION-LOG.md)。

## 其他頂層

| 路徑 | 內容 |
|---|---|
| [docs/](docs/) | 作業系統先驗知識 8 篇（FUSE/VFS、fd 三組、inode/pipe/socket、檔案動詞/process/mmap、page-cache、mount/bind、ptrace/proc-mem、9P 格式）＋[現況-已實作操作.md](docs/現況-已實作操作.md) |
| [ports/](ports/) | 多語言 9P 版：[cpp/](ports/cpp/README.md)、[fennel/](ports/fennel/README.md)——皆通過「Python client ↔ 該語言 server」跨語言互通驗收 ＝**語言中立實證**（規範＝講 9P） |
| OVERVIEW.md / overview.html | 全景文 / 視覺速覽 |

## 設計真源（唯一）

ai_core 筆記鏈（動手前先讀）：世界（OS-as-game）篇 → 兩層底座重解篇 → 實作路線規劃篇。
路徑：`../../workflows/notes/20260719-1150 / -1301 / -1518-*.md`。

## 活狀態

[SESSION-LOG.md](SESSION-LOG.md)（我手上進度）、[WAIT_USER.md](WAIT_USER.md)（待你親自做）。
[DEV-GUIDE.md](DEV-GUIDE.md)＝結構整理被動參考（含 <100 行硬規）。
