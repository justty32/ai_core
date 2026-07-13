# 軸 6 `interruptible` — defs 不足審查

> 審查對象：`notes/axis_6_interruptible.md` Round 2 定案。
> 結構：`unsigned level = 0`（開放碼表，0=unsafe/1=safe/2=resettable/3=rollback/4=resumable/5=graceful，≥6 自定義）+ extra。

這是**安全攸關軸**（決定 hub 能不能 kill），所以表達不足的代價比別軸高。由強到弱：

---

## 1.〔信任級別被抹平〕zero-init 讓「沒想過」與「確定 unsafe」不可區分

`level=0=unsafe` 靠 zero-init。漂亮，但代價：**「作者根本沒考慮中斷性」和「作者審慎判定為 unsafe」塌縮成同一個 0**。
對一個安全軸，這兩者是**不同的信任級別**：
- 沒填 = 我不知道這工具被 kill 會怎樣（最該保守對待）；
- 明確 unsafe = 我確認它會壞。

別的用 `optional` 的軸能靠「缺席 vs 有值」區分，這軸用 zero-init 就壓掉了。
hub 想對「未宣告」者更保守（例如先不 kill、先問），現在做不到——它看不出哪個是「未宣告」。
見索引跨軸主題 3。

## 2.〔序被破壞〕開放碼表混了「有序階梯」與「不可比的擴充值」

碼 0–5 是一條**有序階梯**（數字越大越可恢復/越溫和）。但筆記允許 ≥6 放 `conditional`（權威物件形式的舉例）。
問題：`conditional`（視情況可中斷）**不在「損毀↔恢復」這條單調軸上**——它無法擺進 0..5 的序，
塞到 6 也不代表「比 graceful(5) 更強」。於是這個 unsigned **同時聲稱有序、又裝不可比的值**，序語意自相矛盾。

對照軸 7：值集封閉 → 用封閉 `enum class`，乾淨。軸 6 為了塞 conditional 犧牲了序的可比較性。
**建議：把 conditional 這類「不在序上」的型別趕進 `extra`（如 `extra["mode"]="conditional"`），
讓 `level` 維持純粹有序碼表**；或承認碼表只是「具名分類」放棄序語意（但那 hub 就不能用大小比較做決策）。

## 3.〔線性序逼迫二選一〕graceful 與 resumable 其實正交

筆記 Round 1 自承「6 值混了損毀/恢復/kill 三件事」，最後沒拆（怕重蹈 entries 過度拆解，可理解）。
但留個觀察：`graceful`(5, 不能直接 kill、需 cleanup) 與 `resumable`(4, 可 kill、能續跑) **是正交的**——
一個工具可以**既 graceful 又 resumable**。線性碼表逼作者在 4/5 二選一，丟失組合。
這是把多維性質壓進單一線性碼的內在限制，非筆記疏失，但會讓某些工具無法精確自述。

---

## 用它們自己的尺檢驗

軸 6 的 extra 是對的（裝 conditional 細節）。真正的張力在**把開放擴充塞進一個聲稱有序的碼**：
若 hub 用 `level` 的大小比較做 kill 決策（很自然），那 ≥6 的不可比值就**破壞了這個用法的前提**。
要嘛序純粹（擴充進 extra），要嘛放棄用大小比較。兩者不能既要又要。

## 建議

- `conditional`/自定義型別移出碼表、進 extra，保 `level` 0–5 的序純粹。
- 在筆記釘死「hub 是否用 `level` 大小比較做決策」——這決定上一條怎麼做。
- 評估「未宣告 vs unsafe」是否值得區分（安全軸傾向值得）；若值得，考慮 `optional<unsigned>`。
