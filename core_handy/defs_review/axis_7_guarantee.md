# 軸 7 `guarantee` — defs 不足審查

> 審查對象：`notes/axis_7_guarantee.md` Round 1 定案。
> 結構：封閉 `enum class Guarantee : unsigned { none=0, idempotent=1, transactional=2 }` + extra。

**最乾淨、最有理據**的一軸：封閉值集 → 封閉 enum，分歧由權威本身正當化，爭議最少。
不足偏「跨軸」與「關鍵資訊落 extra」。由強到弱：

---

## 1.〔關鍵重試資訊落 extra〕idempotent 的冪等鍵/dedup key 只能進自由字串

權威無物件形式，但本軸保留了 extra 逃生艙。問題：若 hub 要**自動重試** idempotent 工具，
往往需要知道**冪等鍵怎麼帶**（`Idempotency-Key` header？某個 arg？）——
而這資訊現在只能塞 `extra` 的自由字串，hub 無結構化途徑取得。
`guarantee=idempotent` 告訴 hub「可以重試」，卻沒告訴它「**怎麼**安全重試」。
這是索引跨軸主題 1（關鍵資訊沉入 extra）在本軸的實例：對 hub 自動化重試，冪等鍵是「普遍且必須」的，
依升級壓力本該結構化，卻在 extra。

## 2.〔跨軸無意義組合未閉合〕stateless × transactional

筆記註明 guarantee 對 `stateless`（軸 3）通常**無意義**（stateless 本即等同冪等、無外部狀態可回滾）。
但型別層可表達 `stateless + transactional` 這種無意義組合，退回文件約束。
又一個與軸 3×4、軸 2×5 同類的跨軸未閉合。見索引跨軸主題 2。

---

## 用它們自己的尺檢驗

軸 7 的 enum 選擇完全遵守升級壓力與「讓非法狀態無法被表達」（封閉值集的教科書解）。
唯一逆反處是第 1 點：**重試所需的操作性細節（冪等鍵）被降到 extra**——
若 hub 的核心價值之一就是「自動重試冪等工具」，那這份細節按通則該升格，而非躺逃生艙。

## 建議

- 釐清 hub 的重試流程**是否需要冪等鍵等操作細節**；若需要，考慮為其設 opt 欄位（哪怕只在 idempotent 時有意義）。
- stateless×transactional 等無意義組合，併入「總 metadata 跨軸驗證」統一處理（見索引）。
- 本軸其餘維持現狀即可——它是九軸裡濃縮得最好的範本。
