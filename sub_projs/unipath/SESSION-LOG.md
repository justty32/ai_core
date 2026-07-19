# SESSION-LOG — unipath 進度（只列 open）

完成即移除，歷史交給 git log。← [INDEX](INDEX.md)

## step 6 已完成 ✅（見 `up_tick_script.py` / `unipath_world.py`）

- ~~使用者影響改走路徑樹寫入~~ ✅ 影響＝`Env.write`（外部 echo 同一介面），tick 無旁路佇列。
- ~~巢狀 tick~~ ✅ 元素含 `world`＋`rate`，父一拍 → 子跑 rate 拍。
- ~~規則換可定址腳本~~ ✅ NPC 行為＝住在 `/idx/script/data` 的腳本，可 `cat`/`echo` 讀改；tick exec 它。

## Open：往階段二 / 待收尾

- **script 執行沙箱強化**：目前 `exec` 用 `{"__builtins__": {}}`＋白名單，夠試驗田；若要收外來 world，需更嚴的沙箱（或換成受限 DSL / Lisp 表達式）。
- **世界持久化**：把 world 存成真檔案樹（而非記憶體物件），tick 讀寫磁碟 → 呼應筆記「檔案分佈＝空間狀態」。
- **多 NPC 互動 / LOD**：元素間依 `peer` 互相影響的規則、略圖規則（承世界篇 §十三）。
- **ports 的 ctl 寫入**：cpp / fennel 版目前 ctl「收即忽略」（data write-through 完整）；如需對齊 Python 版 set/append/del 再補。
- **真 9P 寫入的 uid/權限、9P2000.L 擴充**（低優先）。
