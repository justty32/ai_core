# WORKFLOWS — unipath 工作流派發器

← [AGENTS.md](AGENTS.md)｜地圖 [INDEX.md](INDEX.md)

你要做某件事 → 從這張表找對應處，讀該處就知道要動什麼。細節不在這裡。

## 你想做什麼 → 去哪

| 意圖 | 去哪 |
|------|------|
| **跑 / 驗某個 step** | [INDEX](INDEX.md)「程式碼佈局」找薄入口，各檔 docstring 有跑法；或 [OVERVIEW](OVERVIEW.md)「怎麼跑」 |
| **改執行態環境邏輯**（導航 / 讀寫 / ctl） | `up_env.py`（＋ `up_world.py` 助手） |
| **改 9P 線格式 / 訊息** | `up_ninep_codec.py`（編碼）→ `up_ninep_ops.py`（分派）→ `up_ninep.py`（server） |
| **改 tick 規則 / 世界** | `up_tick_rules.py`（規則）/ `up_tick.py`（轉移） |
| **加一個語言 port** | 照 [ports/cpp](ports/cpp/README.md) 或 [ports/fennel](ports/fennel/README.md) 樣板；**驗收＝** 啟你的 server、跑 `.venv/bin/python unipath_9p.py selftest <port>` 印出 SELFTEST OK |
| **補作業系統先驗知識** | [docs/](docs/) |
| **接 step 6**（影響走路徑樹 / 巢狀 tick / 規則換腳本） | [SESSION-LOG](SESSION-LOG.md) 有 open 項；設計脈絡在筆記鏈（見 INDEX「設計真源」） |
| **真 9P 核心掛載** | [WAIT_USER](WAIT_USER.md)（需你 sudo） |

都不符 → 看 [INDEX.md](INDEX.md)。

## 規範

- **鐵律**（試驗田、**每檔 <100 行**、先 hack 後回填規範、未確認不 push、全繁中）→ [AGENTS.md](AGENTS.md)。
- **結構整理原則 + 四級成長軌跡** → [DEV-GUIDE.md](DEV-GUIDE.md)（被動，重構時才取用）。
- **活狀態只列 open**：進度 [SESSION-LOG](SESSION-LOG.md)、待使用者 [WAIT_USER](WAIT_USER.md)；完成即移除，歷史交給 git log。
