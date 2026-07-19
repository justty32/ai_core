# SESSION-LOG — unipath 進度（只列 open）

完成即移除，歷史交給 git log。← [INDEX](INDEX.md)

## Open：step 6 待議 / 待做

- **使用者影響改走路徑樹寫入**：目前 `up_tick` 的影響走旁路 pending 佇列；照筆記 §5.5「執行期持續借樹」，更貼切是讓影響直接走 `echo > …/data`，tick 在回合邊界吸收本回合累積的樹寫入 → 「借樹＝影響」合一，不再有第二條旁路。
- **巢狀 tick**：上層目錄的一個 tick 驅動下層元素多次 tick（承筆記 §4.3），目前只單層全樹掃描。
- **tick 規則換可定址腳本**：現在規則硬編在 `up_tick_rules.RULES`（kind→函數表）；往階段二要把每元素的規則換成**路徑上可定址的腳本**（NPC＝腳本），tick 改成 exec 那些腳本。
- **ports 的 ctl 寫入**：cpp / fennel 版目前 ctl「收即忽略」（data write-through 完整）；如需對齊 Python 版的 set/append/del 再補。
- **真 9P 寫入的 uid/權限、9P2000.L 擴充**（低優先）。
