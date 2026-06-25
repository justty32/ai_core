# 軸 2 `lifecycle` — defs 不足審查

> 審查對象：`notes/axis_2_lifecycle.md` Round 3 定案。
> 定案結構：
> ```cpp
> struct Lifecycle {
>   bool persistent = false;                                  // false=one_shot(預設)、true=persistent
>   std::optional<std::map<std::string,std::string>> extra;   // lazy/warm/periodic/streaming/detached…
> };
> ```

預設極性（false=one_shot）是**對的**（多數工具 one_shot），與軸 1 相反但合理，無 hazard。✅
問題集中在「被踢進 extra 的東西其實 hub 要用」。由強到弱：

---

## 1.〔流動模式無家〕streaming 在軸 1、軸 2 之間互相推諉

- 軸 1 Round 3 排除 `mode` 時說：「若有家，較可能歸 lifecycle 或另立流動軸」。
- 軸 2 把 streaming 判為「one_shot 輸出變體」，**以 `extra` 承載**。

結果：streaming 這個對 hub 很關鍵的性質（要不要邊收邊處理、buffer 策略、能否 pipeline）
**落在某軸的自由字串 extra**，且**沒有約定 key**——hub 想 discover「這工具會串流嗎」得盲翻 lifecycle.extra。
兩軸都把它當「別人的尾巴」，於是它沒有正式的家。這是軸 1 點 4 的另一面，根因同一：**流動模式缺正式建模**。

## 2.〔常駐光譜被壓平〕persistent 單一 bool 對 hub 排程資訊不足

權威自己列了會影響排程的子型：
- **lazy/warm**（首呼才啟動、idle 後關）vs **eager daemon**（一開機就佔資源）；
- **periodic**（定時觸發）。

這些對 hub **有實質差異**：lazy server 與 eager daemon 的資源規劃（軸 5 `idle` 記憶體）、
啟動策略、能否預先 spin-up 都不同。全壓進 `bool persistent` + 自由 extra，等於 hub 對「常駐子型」零結構化資訊。

**懸案：hub 排程要不要區分常駐子型？**
- 要 → 依升級壓力通則，lazy/eager/periodic 該升 opt 欄位（如 `optional<enum StartupPolicy>`），不該躺 extra。
- 不要 → 現狀 OK，但需在筆記明說「hub 不分常駐子型」以閉合此題。

## 3.〔判斷可疑〕detached/fire-and-forget 被踢成「呼叫方關切」

筆記把 detached 判為「呼叫方的排程策略」而踢出 lifecycle。但「適合 detached 跑」其實有**工具自身**的成分：
壽命短、無需 stdout 互動、結果落 data 目錄——這些是工具自我描述的維度，不純是呼叫方選擇。
此點較弱（多數情況確是排程決定），但值得標記：別把工具自帶的「可後台化」屬性誤推給呼叫方。

---

## 用它們自己的尺檢驗

同軸 1：若 hub 需要「串流與否 / 常駐子型」來排程，依升級壓力通則它們是「普遍且必須」→ 該升欄位。
現狀把它們全留在 extra，再次押注「hub 不需要結構化的這些」。

## 根源懸案

與軸 1 共用同一根樁：**hub 消費 lifecycle/entries 的哪幾維？**
特別是**「流動模式（batch/streaming/interactive）的正式家在哪」**——目前它在軸 1、軸 2 之間流浪，
建議要嘛在 entries 設正式欄位、要嘛另立「流動軸」，二擇一明確收掉，不要繼續以 extra 暫存。
