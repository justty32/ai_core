# 軸 8 `dry_run` — defs 不足審查

> 審查對象：`notes/axis_8_dry_run.md` 定案。
> 結構：描述層只留 `bool allow_dry_run = false`；object 細節（flag/state_entry/error_entry）全降 impl。

乾淨，但**降級的前提假設（lib 統一約定 flag）對 brownfield 不成立**——這是本軸的核心缺口。由強到弱：

---

## 1.〔brownfield 盲點〕bool 化假設「程式是用本 lib 新寫的」，但 ai_core 大量是馴化既有 CLI

降級理由是「lib 提供 dry-run 設施 → flag 名稱由 lib 統一約定，程式只需宣告布林」。
這對**用本 lib 新寫（greenfield）**的程式成立。但對**包裝既有程式（brownfield）**就破：

| 既有程式 | dry-run flag | lib 能統一約定嗎 |
|---|---|---|
| `git clean` | `-n` | ❌ git 不甩 lib |
| `rsync` | `--dry-run` | ❌ |
| `terraform` | `plan`（子命令！） | ❌ |

這些 flag **各不相同、且不在 lib 控制下**。`bool allow_dry_run` 等於只告訴 hub「它支援乾跑」，
**卻丟失了「乾跑要傳什麼」**——而 hub/笨模型實際要用的正是這個。
權威的 object 形式（`flag:"-n"`）正是為描述既有程式而存在。
→ 濃縮成 bool **只服務 greenfield、放棄 brownfield 包裝**。

但 roadmap 的「shell/app 作為函式」「馴化既有框架」恰恰**大量是 brownfield**。
這使本軸的 bool 化與 roadmap 的現實衝突——很可能是真缺口，不只理論。

## 2.〔預覽輸出去哪也丟了〕state_entry/error_entry 降級同病

「乾跑時『會發生什麼』印到哪個 entry」原由 `state_entry`/`error_entry` 指定（引用軸 1 的 key）。
降 impl 後假設 lib 接線。但包裝既有程式時，預覽就是印到**它自己的 stdout**，
hub 要知道**去哪讀預覽**。這份接線資訊同樣蒸發了。
→ 與軸 1 點 3（傳輸身分缺失）是同一個 brownfield 盲點的延伸。

---

## 用它們自己的尺檢驗

對 greenfield，`bool` 完全遵守「lib 標準化掉細節」的精神，無懈可擊。
對 brownfield，flag/輸出接線是「普遍且必須」（hub 不知道 flag 就無法觸發乾跑）→ 依升級壓力該保留結構化欄位。
**根本矛盾：濃縮假設了一個 lib 全控的世界，但 ai_core 要馴化的是 lib 不控的既有 CLI。**

## 建議

- 為 brownfield 包裝保留**選填**的 flag/輸出接線描述（即使 greenfield 用不到）。
  候選：`bool allow_dry_run` 保留為快速判斷，另在 extra 或 opt 標 `dry_run.flag` / `dry_run.preview_entry`。
- 這題與軸 1 transport、索引跨軸主題 4（brownfield 盲點）綁在一起，建議一起決：
  **defs 要不要正式承認「包裝既有程式」這一類，並為它保留描述既有介面的欄位？**
