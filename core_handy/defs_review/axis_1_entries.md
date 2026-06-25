# 軸 1 `entries` — defs 不足審查

> ✅ **已處理（Round 12，見 `notes/axis_1_entries.md`）**：bool → 三張開放整數碼表
> `direction`(0=in/1=out/2=in_out) + `content`(0=binary/1=text/≥2擴充) + `access`(0=readonly/1=writable)。
> - #1 數值通道 → content 碼 ≥2（整數碼表還魂 Round 7）✅
> - #2 方向回歸 → direction 與 access 分立，in-place 可表達 ✅
> - #5 預設極性 → 使用者選 0=binary（相容優先，不修極性）✅
> - #3 傳輸身分、#4 流動模式 → **延後 hub 階段**（現以 map-key + extra 暫存）。
> 下方為原始審查全文，保留。

> 審查對象：`notes/axis_1_entries.md` Round 11 定案。
> 定案結構：
> ```cpp
> struct Entry {
>   bool text = false;       // 人讀(text+json,utf-8) vs 二進位；預設二進位
>   bool writable = false;   // 可寫 vs 唯讀；預設唯讀
>   std::optional<std::map<std::string,std::string>> extra;
> };
> using Entries = std::map<std::string, Entry>;  // 具名通道 → entry
> ```

對照權威（`_core.py` 的 `entries`、`kb-ss/doc_05`「水管接哪裡＋水怎麼流」、`doc_06` lib 契約）。由強到弱：

---

## 1.〔回歸〕Round 11 重開了 Round 7 已解掉的「數值/狀態通道」邊

Round 3 標記過一個邊：exit code / HTTP status / errno 這種小整數通道，套「text/binary」會繃。
Round 7 用「內容＝自由字串」**明確溶掉**它（寫 `"int"`）。但 Round 10/11 把 content 壓回 `bool text`，
於是這類通道又無家可歸：`text=false`(二進位)語意錯、`text=true`(人讀)也勉強。

**這是一個被自己解過、又被後續重設計悄悄打回的回歸**，而筆記沒有重新標記。
Round 2 窮舉出的 `stderr`(診斷側通道)、exit code(窄狀態通道)等角色，現在只能塞 `extra`。

## 2.〔結構性〕方向被 `writable` 吞掉，對「檔案/URL 通道」會破

`writable≈輸出、readonly≈輸入` 這條啟發式**只在純 pipe（stdin/stdout）成立**。
Round 2 自己窮舉指出真正有趣的是 indirection 通道（檔案路徑/URL）——而那裡方向與「改不改資源」正好分岔：

- 讀 config 檔：輸入 + 不改 → `writable=false` ✅
- 寫新檔：輸出 + 改 → `writable=true` ✅
- **in-place 改檔（`sed -i`）：又讀又寫（InOut）** → `writable` 只能表達「會改」，**丟失「同時也是輸入」**。

`writable` 把兩個正交問題（方向 vs 是否變動後端資源）摺成一個 bool，恰在它們分岔的場景失真。
Round 5 自承「in-place 原地改暫不表達」——但這正是檔案工具的常態。

## 3.〔組合斷層〕通道「傳輸身分」不在 defs，但 hub 組合需要

`doc_05` 說軸 1 要回答「水管接哪裡」（stdin？檔案？http？）。但定案把傳輸身分整塊外包給 impl 層
（筆記末段明說「不在 defs 的 Entry」）。後果：hub 從 `--metadata` JSON **看不出某通道是 stdin 還是檔案還是 tcp**，
唯一把手是 `map` 的 key（語意未定義的自由字串）。但 hub 要把 A 的 stdout 接到 B 的 stdin，
**沒有傳輸身分就無法配對組合**。描述面說「給 hub 看」，卻拿掉了 hub 組合最需要的那一維。

## 4.〔語意去向〕流動模式（batch/streaming/interactive）整個消失

`doc_05` 的「水怎麼流」現落在 `lifecycle.extra`（開放問題第 2 條）。問題：
(a) 它跑去**別的軸**的 extra，不在 entries，hub 要看流動性得跨軸翻自由字串；
(b) interactive 其實是「通道」性質不是「生命週期」性質，安置位置可議。
→ 與軸 2 的同一懸案互為表裡（見 `axis_2`）。

## 5.〔預設極性〕`text=false`/`writable=false` 與 Round 9「不藏意圖」自相矛盾

最常見通道（stdin/stdout）是**文字**，但預設是 binary；忘填一個 bool → 靜默標成二進位，hub 會誤判。
Round 9 當初**移除 entries 預設值**正是為了「不藏隱含意圖」，Round 11 卻反手加了**極性偏向少數情況**的欄位預設。
對信奉「讓非法/錯誤狀態難以表達」的線，「會靜默錯」的 bool 預設比「必填」更危險。
另一面：`text`/`writable` 用 zero-init bool → **「沒填」與「明確宣告 binary/readonly」不可區分**（見索引跨軸主題 3）。

---

## 用它們自己的尺檢驗（升級壓力）

`00_index` 的「升級壓力」通則說：**普遍且必須 → 升成固定欄位**。
若 hub 組合**需要**方向 / 傳輸身分 / 流動模式，依此通則它們就該是**固定欄位**，而非躺 `extra` 或散到別軸。
當前定案等於賭「hub 不需要它們」——但組合故事（`doc_13` hub）尚未把這賭注講清楚。

## 根源懸案

**hub 到底吃 entries 的哪幾維來組合？** 把這題定了，第 2/3/4 點的去留會自動落定——它們全釘在這根樁上。
建議先對 `doc_13_arch_hub.md` / `composite_spec` 求證 hub 的真實消費維度，再回頭調 entries。
