# SESSION-LOG — unipath 進度（只列 open）

完成即移除，歷史交給 git log。← [INDEX](INDEX.md)

## step 6 已完成 ✅（見 `up_tick_script.py` / `unipath_world.py`）

- ~~使用者影響改走路徑樹寫入~~ ✅ 影響＝`Env.write`（外部 echo 同一介面），tick 無旁路佇列。
- ~~巢狀 tick~~ ✅ 元素含 `world`＋`rate`，父一拍 → 子跑 rate 拍。
- ~~規則換可定址腳本~~ ✅ NPC 行為＝住在 `/idx/script/data` 的腳本，可 `cat`/`echo` 讀改。
- ~~規則語言換嵌入式 Lisp~~ ✅ 已換 **Janet**（`up_tick_janet.py`）——比 Python `exec` 更貼「規範層語言中立」、獨立行程天然沙箱。

## Open：往階段二 / 待收尾

- **Janet 引擎優化**：目前每元素每回合起一個 janet 行程（JSON 交換），暫不管效能；日後可換常駐 janet REPL / libjanet 嵌入。
- **script 能力擴充**：目前 Janet 腳本可讀 `peer`（快照）、改自身純量欄位；日後開放更多（呼叫世界 API、觸發事件）。
- **世界持久化**：把 world 存成真檔案樹（而非記憶體物件），tick 讀寫磁碟 → 呼應筆記「檔案分佈＝空間狀態」。
- **多 NPC 互動 / LOD**：元素間依 `peer` 互相影響的規則、略圖規則（承世界篇 §十三）。
- **ports 的 ctl 寫入**：cpp / fennel 版目前 ctl「收即忽略」（data write-through 完整）；如需對齊 Python 版 set/append/del 再補。
- **真 9P 寫入的 uid/權限、9P2000.L 擴充**（低優先）。
