# 軸 3 `state` — defs 不足審查

> 審查對象：`notes/axis_3_state.md` Round 2 定案。
> 定案結構：`bool stateful = false;`（false=stateless 預設、true=stateful_external）

這軸本身**最乾淨**：二元、強預設安全端、無 detail，無單軸內爭議。✅
不足全在「二元太粗」與「跨軸未閉合」。由強到弱：

---

## 1.〔讀 vs 寫被摺平〕只讀外部狀態被迫二選一站錯邊

權威語意是「對外部狀態**讀寫**」。但「跨呼叫副作用」的真正關切是**寫**：
- **只讀外部**（讀 config / 讀資料庫不改）：**無副作用**、可安全重試、可平行——行為更像 stateless。
- **寫外部**（append log / 改 DB）：有副作用——這才是 stateful 真正要警示 hub 的。

`bool stateful` 把這兩者混為一談。後果：一個只讀 config 的工具，
標 `stateful=true` 會**過度保守**（hub 誤以為不可平行/有副作用），標 `stateful=false` 會**漏報外部依賴**。
兩種填法都不準。這與軸 1 `writable` 把「方向＋是否變動資源」摺平是**同一個「讀 vs 寫」張力的另一面**。

## 2.〔跨軸冗餘且無約束〕「有無外部副作用」同時被軸 1、軸 3 表達

- 軸 1 某 entry `writable=true` ＝ 會改某通道背後資源 ＝ 有外部副作用。
- 軸 3 `stateful=true` ＝ 有外部狀態副作用。

同一事實被兩軸各自表達，**型別層無一致性約束**：可以出現「某 entry writable=true 但 stateful=false」的矛盾，
hub 收到互相打架的 metadata 也沒人擋。建議定義兩軸的權威關係（誰主誰從、能否互推），或明說允許不一致。

## 3.〔跨軸非法組合未閉合〕stateless × state_dirs

軸 4 自己的 D3 討論過：**stateless 卻宣告了 state_dirs** 是非法組合（無狀態哪來狀態目錄）。
傾向方案 (b)（把 dirs 巢進 `optional<StateDirs> stateful` 讓非法組合無法表達），
但因軸 3 已先拍 `bool`，最終停在方案 (a)（兩軸獨立、靠文件約束）。
→ C++ 線的賣點「讓非法狀態無法被表達」在這對軸上**被放棄**，且是**已知未閉合**。
要嘛接受 (a) 並明文標「此處放棄型別保證」，要嘛回頭採 (b)。見索引跨軸主題 2。

---

## 用它們自己的尺檢驗

軸 3 不違反升級壓力（二元就是最小必須）。它的問題是**語意切點**：
把「讀 vs 寫」切在 stateful 之外，可能讓這個給 hub 判斷「能否平行/重試」的關鍵軸給出失真答案——
而能否平行/重試正是 hub 排程最想從這軸拿到的。切點錯，欄位再簡潔也沒用。

## 建議

- 評估是否要把 stateful 細分為「唯讀外部依賴 vs 會寫外部」（哪怕只在 extra 標 `external:"read"|"write"`）。
- 定義軸 1 `writable` 與軸 3 `stateful` 的權威關係，閉合冗餘。
