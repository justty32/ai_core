# 軸 6+7 impl：transaction helper（合併設施）

> 狀態：✅ 設計拍板（Round 1+2，2026-06-27）。實作待落地（`impl/io.hpp` 的 `write_atomic` + StateStore 改用）。
> impl 金字塔 L2（見 `impl_overview.md`）。建在 L0 軸 1 I/O 上。

## 為什麼軸 6 與軸 7 合併

兩軸 notes 的 def/impl 定位都各自寫了「高度重疊、歸同一設施」：

- **軸 7 transactional**（執行保證）：全成功或全不發生，中途失敗不留部分狀態。
- **軸 6 rollback**（可中斷性）：中斷後完整還原至呼叫前。

**這是同一個機制的兩面**（軸 7 notes：「rollback × transactional 同向」）。差別只在用什麼機制做到
「全有或全無」——而那個機制**一套就同時供兩軸**。

---

## Round 1（✅ 核心機制：原子寫）

POSIX `rename()` 是**原子**的 → 最 KISS、no-wheel-remake 的事務原語：

```
寫到暫存檔 tmp → rename(tmp, 目標)        // commit
中途崩潰/中斷 → 還沒 rename → 目標原封不動   // 自動 rollback
```

**一個機制同時交付軸 7 transactional + 軸 6 rollback**——「還沒 commit 就中斷＝原檔不動」本身就是回滾。
這正是兩軸該合併的根本：它們共用 `rename` 這把鎖，把「all-or-nothing」這件難事交給 OS。

### 範圍（目標問題＝停止鍵）

第一目標問題（笨模型在標記位置填碼、retry/guard）的事務需求＝「**一次編輯要嘛完整套用、要嘛原檔不動**」
＝**單檔原子寫**。多檔真 ACID（跨檔一起 commit）**現在零消費者** → 延後（同 stream 群的延後邏輯）。

- ✅ v0：單檔原子寫原語。
- ⏸ 延後：多檔暫存 Txn 物件（崩在順序 rename 中間＝部分，真多檔原子難；無消費者）。
  **不是死路**：屆時 `write_atomic` 正是它的 commit 原語。

---

## Round 2（✅ 細節拍板）

### ① 簽名與放哪

```cpp
namespace ac::io {
  // addr=="-"（串流）→ 退回 write_all（串流無原子語意）。
  // 否則：寫 <addr>.tmp（**同目錄**）→ std::filesystem::rename(tmp, addr)。
  void write_atomic(const std::string& addr, std::string_view data);
}
```

`write_all` 的兄弟（同在 L0 io），建在 `write_all` 上（tmp 也用 write_all 寫）。

- **tmp 必須與目標同目錄**（`addr + ".tmp"`）：rename 才在同檔系統內＝真原子；放 `/tmp` 會變跨 FS
  copy+delete、破壞原子性。
- `addr=="-"`：串流無原子語意，退回 `write_all`（文件註明）。

### ② fsync：v0 不做

`rename` 本身原子——「不會出現半截/壞檔」這個**重要保證**不需 fsync 就有。fsync 多保證的是斷電後
資料已落碟（durability）。但：(a) 要 fsync 得掉到 POSIX `<unistd.h>` 拿 fd，破壞「乾淨建在 write_all 上」；
(b) 用例（程式碼編輯、狀態託管）**在意原子性遠勝斷電持久性**。

→ **v0 不 fsync**，靠 rename 原子性；durability/fsync 留註記延後。
**單寫者假設**：tmp 用固定 `.tmp` 後綴；並發多寫者的唯一命名（pid/亂數）延後，文件註明。

### ③ StateStore.save 全面改用 write_atomic

`StateStore.save`（單檔與資料夾兩多載）內部改走 `write_atomic`。代價可忽略（一次 temp+rename），
換到**託管狀態天生原子**——state/data 檔永不因中斷半截，正對軸 4「data 需保護」語意。
等於 StateStore 免費獲得軸 6 rollback + 軸 7 transactional。

### ④ resumable / idempotent：v0 只文件化、不寫碼（YAGNI）

兩個都是 StateStore 上的薄樣式，v0 不預先做 helper：

- **軸 6 resumable**（從斷點續跑）＝ 進度標記存 StateStore `state` 目錄，重跑讀回續做。
- **軸 7 idempotent**（去重）＝ 處理過的 key 存 StateStore，命中即跳過。

真需求出現再升成 helper；現在寫成「使用樣式」即可。

---

## 落地清單（給實作）

1. `impl/io.hpp` 新增 `ac::io::write_atomic`（建在 `write_all` + `std::filesystem::rename`）。
2. `impl/state.hpp` 的 `StateStore::save`（兩多載）改用 `write_atomic`。
3. resumable/idempotent 樣式：寫進本筆記「使用樣式」段（上方④已含，實作不需動碼）。
4. demo：示範「寫到一半模擬中斷、原檔不動」或至少「save 後檔案完整」；確認 `--metadata` 不受影響。

## 待續 / 延後
- 多檔暫存 Txn 物件（無消費者，延後）。
- fsync / durability（延後）。
- 並發多寫者的 tmp 唯一命名（延後）。
