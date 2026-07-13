# 軸 8：`dry_run`（乾跑）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊：描述層 `bool allow_dry_run`）。flag/輸出接線等細節屬 `impl/`。本檔保留設計脈絡。

> 狀態：✅ 定案（Round 2 複查確認，bool 不動）。描述層只留 `bool allow_dry_run = false`；
> object 細節（flag/state_entry/error_entry）全降級到實作層。
> R2 釘死前提：**ac_helper = greenfield 創作 lib，brownfield 一律經 wrapper 進來** → bool 夠（見下 Round 2）。

---

## 0. 是什麼

**dry-run（乾跑／空跑）＝「只演練、不真做」**：程式照跑完整邏輯、算出「**我將會做什麼**」並輸出，
但**不真的修改任何外部狀態**（不寫檔、不改 DB、不送 API）。供呼叫方預覽副作用、確認後再真的執行。

例：`git clean -n`、`rsync --dry-run`、`terraform plan`。在 ai_core 的世界，這是 hub／笨模型操作
不完全信任的程式時的**安全閥**——先看一眼「這指令到底會幹嘛」。

---

## 1. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_validate_dry_run`：`bool | dict`）、`docs/spec/lib_spec.md §6`、
`docs/spec/axis_spec.md §6`（規範上與 guarantee 同列 §6，但 `_KNOWN_FIELDS` 為獨立欄位）。

- 型別 `bool | object`：bool `true`＝支援；object＝`{flag, state_entry, error_entry}` 三子欄位皆選填。
- **預設**：缺席／`false` → 不支援。
- **是「能力宣告」非當前狀態**：呼叫方依此決定要不要傳對應 flag。
- object 子欄位：`flag`＝觸發乾跑的 CLI flag；`state_entry`/`error_entry`＝引用 `entries`(軸1) 的 key，
  指定乾跑時「會發生什麼」/錯誤輸出到哪個 entry。

---

## 2. 定案（描述層只留一個 bool）

| 決策 | 選擇 | 說明 |
|---|---|---|
| **描述層型別** | **`bool allow_dry_run = false`** | hub 在描述層只需「支不支援」這個布林事實。 |
| **預設** | **`false`（不支援）** | 沿用軸 2/3 強預設到「簡單／否」那端；zero-init 即 false。 |
| **object 細節去向** | **全降級到實作層** | flag/state_entry/error_entry 是「怎麼觸發、預覽印去哪」的接線，由 lib 的 dry-run 設施標準化，不要求每支程式各自宣告。 |
| **opt / extra** | **無** | 同軸 3 純 bool flag，描述層不掛 opt、不掛 extra。 |

### 為何 object 細節歸實作層
lib 的目的是「依託它寫的程式自動符合標準」。若 lib 提供 dry-run 設施（解析 flag、把預覽導向約定的
輸出口），則 **flag 名稱與輸出接線由 lib 統一約定**，程式只需宣告「我支援」這個布林。
把 flag/state_entry/error_entry 留在描述層，等於要每支程式重複宣告 lib 已經標準化的東西。
→ 描述層極簡化為 `bool`，細節由設施承載。

### 型別草圖（定案）

```cpp
bool allow_dry_run = false;   // false=不支援(預設)、true=支援乾跑
```

三層對位：固定＝`allow_dry_run`；opt＝無；額外＝無。與軸 3 `bool stateful` 同形（最乾淨的一類軸）。

> 是否獨立成 struct 還是內聯進總 metadata，留全軸收尾統一決定（與軸 3 一致）。

---

## def/impl 定位（2026-06-24）

- **描述面（→ defs）✅**：`bool allow_dry_run`——只回答「支不支援乾跑」。
- **設施面（→ impl）**：承載被降級的所有細節，**待之後集中設計**：
  - **flag 約定**：lib 統一的乾跑觸發 flag（如 `--dry-run`），程式不必自訂。
  - **`is_dry_run()` 查詢**：解析該 flag，給程式本體一個布林查詢「現在是不是乾跑」。
  - **輸出接線**（原 state_entry/error_entry）：乾跑時把「會發生什麼」/錯誤導向約定輸出口
    （與軸 1 entries 接線相關）。
  - 真正「不碰外部狀態」仍是 app 邏輯，lib 難代勞；設施只負責 flag 解析與輸出導向。
  - 與軸 1（entries 接線）、軸 7（guarantee／transaction）關聯。**僅定位，待 impl 集中重做設計。**

---

## 開放問題 / 後續輪次
- 設施層 dry-run flag 約定、`is_dry_run()` API、預覽輸出接線——待 impl 集中重做設計時一併處理。

---

## Round 2（✅ 複查確認 bool 不動，defs 重新思考 2026-06-27）

對照 `defs_review/axis_8_dry_run.md`。review 核心：bool 對 greenfield 完美，但 git `-n` / rsync
`--dry-run` / terraform `plan` 各不同、lib 管不到，bool 丟失「乾跑要傳什麼」——brownfield 盲點。

### 化解：靠 ac_helper 的定位，不靠加欄位

關鍵反問：**git 會直接帶 `--metadata` 出現在系統裡嗎？不會。** ac_helper 定位是「作者依託此 lib
寫 shell 程式」（greenfield 創作 lib）。git 沒有 `--metadata`、進不了系統，除非有人寫一個**用 ac_helper
的 wrapper** shell out 給它。於是：

- 有 `--metadata` 的實體永遠是 wrapper（greenfield ac_helper 程式），不是 git。
- wrapper 對外用 lib 標準乾跑 flag（`--dry-run`），對內翻譯成 git 的 `-n`。
- **git 的 `-n` 是 wrapper 的 impl 細節，不是 defs。** 消費者只看到 lib 標準 flag，靠 bool 就知道怎麼觸發。

review 設想的「object form 描述 git `-n`」對應的是另一個模型（ac_helper 直接描述既有 CLI），
**那不是 core_handy 的模型**。state_entry/error_entry 同理：wrapper 把 git stdout 接到 lib 約定預覽口，
接線是 wrapper 的 impl。

### 前提已拍（2026-06-27 使用者選「前」）

**ac_helper = greenfield 創作 lib，brownfield 一律經 wrapper 進來。** → bool 夠，brownfield flag/接線
歸 wrapper impl，不進 defs。此前提同時收掉 defs_review 索引「主題 4 brownfield 盲點」，並回頭確認
軸 1 不必為 brownfield 保留傳輸/介面欄位（見 `00_index.md` 全域立場）。

### 型別（定案不變）

```cpp
bool allow_dry_run = false;   // false=不支援(預設)、true=支援乾跑
```

---

## 設施 Round（is_dry_run 落地，2026-06-27）

描述面只回答「支不支援」（`bool allow_dry_run`）；**設施面**負責「怎麼觸發查詢」。落地於
`impl/cli.hpp`（namespace `ac::cli`）。

### 決定

```cpp
namespace ac::cli {
  bool is_dry_run(int argc, char** argv);  // 掃 argv 有無 --dry-run，回 bool
}
```

- **flag 約定 ＝ `--dry-run`（lib 統一、寫死）**：呼應 greenfield-wrap 模型（Round 2）——對外永遠是
  lib 標準 flag，程式不自訂；brownfield 的 `-n`/`--dry-run` 差異由 wrapper 內部翻譯，不進此設施。
  因為 flag 名固定，`is_dry_run` 不需參數化 flag（與 `resolve` 不同：resolve 一程式多通道、需傳 flag；
  dry-run 全 lib 一個約定、寫死最 KISS）。
- **純查詢、零副作用**：給程式本體一個布林「現在是不是乾跑」。真正「不碰外部狀態」仍是 app 邏輯
  （設施代勞不了）——main.cpp 示範即 `if (dry) { 印預覽 } else { 真寫 StateStore }`。
- **風格**：比照 `intercept.hpp`/`resolve` 線性掃 argv，邊界安全。

### 延後（KISS，明確不做）

- **預覽輸出導向**（原 state_entry/error_entry：把「會發生什麼」/錯誤導向約定輸出口）**先不做**。
  目前 app 自己決定預覽往哪印（demo 直接 `std::cout`）。等真有消費者逼出「標準預覽口」需求，
  再接到軸 1 entries（屆時很可能就是 `resolve` 出一個 preview 位址 + `write_all`）。不憑空造。
