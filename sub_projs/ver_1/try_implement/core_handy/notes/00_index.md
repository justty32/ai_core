# core_handy 設計筆記 — 索引

> # ⚠️ 收斂公告（2026-06-28）：本目錄已降為**歷史討論記錄**
>
> **事實基準（現況、唯一可信來源）已遷出 notes：**
> - **描述面九軸** → [`../defs/axes.hpp`](../defs/axes.hpp)（已逐軸驗證：notes 最終拍板 100% 對齊）
> - **設施面設施碼** → [`../impl/*.hpp`](../impl/)
>
> `notes/` 自此**只保留設計如何走到定案的 WHY/脈絡**，不是查「現在長怎樣」的地方。各軸 note 已抽段：
> 只留**最終拍板 + 現況脈絡**，被 supersede 的舊輪次移入 [`archived/`](archived/)（見 [`archived/README.md`](archived/README.md)）。
> 已結案的 defs 審查 `defs_review/` 亦整批封存於 `archived/defs_review/`。
>
> 下方「九軸進度表」等內容**已凍結**，反映截至 2026-06-27 定案時的討論狀態；與 `defs/axes.hpp` 衝突時，**一律以 `defs/` 為準**。

## 這是什麼

`core_handy/` 是 ai_core 現有設計的一次**嚴謹濃縮 demo**：用 C++（最嚴謹、使用者最熟）把現有設計
一軸一軸重新審視，濃縮成最精煉的形狀。**每一步都經使用者確認**；會經歷多輪討論，最終才進入實作。

- **產物目標**：`ac_helper.hpp`（header-only）＋ `main.cpp`（示範）。
- **與 `src/ai_core` 的關係**：那條線是「純 Python 標準庫」；`core_handy` 是另一條刻意分開的 C++ 濃縮線。
- **權威來源**：濃縮的對象是 `src/ai_core/_core.py`（九軸驗證）、`docs/spec/`（規範）、`roadmap.md`（北極星）。
  濃縮時若與權威來源分歧，會在筆記裡標明「為何分歧、在哪邊還原」。

## ★ lib 是什麼：輔助寫「符合標準的 shell 程式」（2026-06-24 定錨）

`ac_helper` 的**目的**：作者**依託此 lib 寫 shell 程式**，產物即**自動符合九軸標準定義**。
這是 `src/ai_core/_core.py` 的 `register()`／`intercept()` 模型的 **C++ 對應**。

| 層 | 對作者 | 對外（hub / LLM） |
|---|---|---|
| **defs（描述）** | 作者**填宣告**（三層欄位：必須/推薦/額外） | 被**序列化成 `--metadata` JSON** 給 hub 讀 |
| **impl（設施）** | 作者**選用託管**（狀態目錄…），少寫輪子 | 行為符合慣例（副作用範圍可預測） |
| **膠水** | `main()` 裡呼叫一個 intercept 級進入點 | 命中 `--metadata` 吐 JSON 並 exit，否則交還控制 |

推論：**`--metadata` 序列化是 defs 層的核心職責**（不是可選 demo）——lib 的存在意義就是產出
「會正確回應 `--metadata` 的 shell 程式」。C++ 版進入點草圖（筆記層，先不寫碼）：

```cpp
int main(int argc, char** argv) {
    ac::Meta meta{ /* 九軸型別拼成 */ };
    ac::intercept(argc, argv, meta);   // 命中 --metadata → 吐 JSON + exit；否則 return 續跑本體
    ... // 程式本體
}
```

## ★ 三層 ↔ 三欄位通則（2026-06-24 確立，治所有 defs/ 型別）

定義的重要度分三級，一比一對應 C++ 型別的欄位種類：

| 定義層 | 實作欄位 | C++ 形狀 | 性質 |
|---|---|---|---|
| **最小必須** | 固定欄位 | `T field = default;`（純值，恆在） | hub 永遠可依賴；KISS 那端 |
| **推薦** | opt 欄位 | `std::optional<具體型別>` | 結構化、有型別，但可缺席 |
| **額外** | extra 欄位 | `std::optional<std::map<std::string,std::string>>` | 自由字串袋，最低保真，逃生艙 |

- **extra 故意低保真**（string→string）：巢狀資料須壓平成字串 → 刻意製造張力，逼重要欄位往上升級。
- **升級壓力**：`extra →（常見且重要）→ opt 欄位 →（普遍且必須）→ 固定欄位`。
  這條路**一次回答所有「某欄位該放哪」的懸案**（mode/streaming/terminal_binding…）：先進 extra，夠格再升。
- **三槽選用**：不是每軸都填滿。可只有固定（軸 3 `bool stateful`），或跳過 opt 直接到 extra（軸 1）。
- **三層直接塑形 `--metadata` JSON**：固定＝必出鍵、opt＝有才出。

> **★ 2026-06-28 再收斂（此節部分作廢，權威以 [`../defs/axes.hpp`](../defs/axes.hpp) 為準）**：
> - **extra 不再 per-axis** → 全軸收斂成**單一 `Meta::extra`**。上表「額外/extra 欄位」整個 Meta 只剩一個；各軸只填固定/opt 兩槽。
> - 原為**單欄位**的軸（2 lifecycle / 7 guarantee / 9 nondeterministic）已**內聯成 Meta 裸欄位**（同軸 3 stateful / 軸 8 allow_dry_run），不再包 struct。
> - 序列化據此變：內聯軸出扁平鍵（`"persistent"` / `"guarantee"` / `"uncertainty"`）；extra 出頂層 `"extra":{…}`（有值才出）。

## ★ 兩層區分：標準定義 vs 標準實作庫（2026-06-24 確立）

這是讀懂 `core_handy` 內容構成的關鍵框架：

- **九軸標準定義（spec）**：目標是對函數狀態做**統一描述**。純描述性。在這層，程式狀態該由程式自管，
  描述性軸只到「軸 3：函數有沒有外部狀態（`stateful`）」。**軸 4 不屬於描述性九軸。**
- **標準實作庫（＝ `core_handy` / `ac_helper`）**：依託九軸標準、建在其上。它有**兩類內容**：
  1. **描述性 metadata 型別**（軸 1/2/3…）——描述一個函數「是什麼」。
  2. **提供的設施／實作**——不描述，而是「替程式做掉」的功能碼。**軸 4「狀態統一託管」是第一個例子**：
     標準目錄慣例（config/cache/state/data）的現成實作（路徑解析、建目錄、JSON 讀寫），程式選擇性託管。

> 推論：本筆記的「九軸進度表」其實混了兩類東西。描述性軸做的是「濃縮型別」；設施軸（軸 4…）做的是「設計 API」。

## 工作節奏

1. 一軸一軸來，順序＝`_core.py` 的 `_KNOWN_FIELDS`。
2. 每軸先**攤定義 → 提案濃縮 → 多輪討論 → 拍板**，過程記在該軸的筆記檔。
3. 全部軸拍板後，才動手寫 `ac_helper.hpp` 與 `main.cpp`。
4. 筆記用詞：**討論中**＝未定；**Round N**＝第幾輪；**✅ 拍板**＝使用者確認。

## 慣例（暫定，討論中）

- 語言標準：C++20（concepts / designated initializers 可用，利於嚴謹約束）。已驗證可編。
- 工具鏈：MinGW64 g++ + CMake（`MinGW Makefiles` generator）。
- 設計原則繼承 roadmap：KISS / Lightweight / No wheel-remake / Least dependency。
  C++ 版額外加一條：**讓非法狀態無法被表達**（make illegal states unrepresentable）。

## 九軸進度表

順序出自 `_core.py` 的 `_KNOWN_FIELDS`。

> **🎉 描述面收尾（2026-06-24）**：軸 1/2/3/5/6/7/8/9 描述面全部濃縮定案；軸 4 為純設施（描述面外包給軸 3）。
> 下一階段：回頭**重看全部 impl 筆記、統一重做實作設計**（軸 1 統一 I/O 為地基 → 軸 2 serve、軸 4 狀態託管、
> 軸 5 rate-meter、軸 6/7 transaction、軸 8 dry-run plumbing、軸 9 馴化框架），再寫 `ac_helper.hpp` + `main.cpp`。

| # | 軸 | 筆記檔 | 狀態 |
|---|---|---|---|
| 1 | `entries`（I/O 出入口） | [axis_1_entries.md](axis_1_entries.md) | ✅ 定案（**Round 14，Unix 統一 I/O，supersede R13**）：`unsigned direction`(in/out/in_out) + `unsigned flow`(0=batch/1=streaming) + `unsigned content`(binary/text/≥2)；transport 種類退出描述→runtime 位址 scheme、統一 read/write；mutation→軸3、extra→Meta::extra |
| 2 | `lifecycle`（生命週期） | [axis_2_lifecycle.md](axis_2_lifecycle.md) | ✅ 定案（R4 複查）：`bool persistent`(預設false=one_shot)；**已內聯成 Meta 裸 bool、extra 上收 Meta（2026-06-28）**；流動模式正式判給軸 1、軸 2 不收（止住跨軸流浪） |
| 3 | `state`（跨呼叫狀態） | [axis_3_state.md](axis_3_state.md) | ✅ 定案：單一 `bool stateful = false`（false=stateless 預設、true=stateful_external），無 detail |
| 4 | `state_dirs`（狀態目錄） | [axis_4_state_dirs.md](axis_4_state_dirs.md) | 討論中（Round 1 提案） |
| 5 | `resources`（資源特性） | [axis_5_resources.md](axis_5_resources.md) | ✅ 定案（R3，砍 cpu）：無固定欄位、opt 預定義(memory/gpu/time/disk/network)；**extra 上收 `Meta::extra`（2026-06-28，自定義依賴/cpu 改走 Meta::extra）**；network 帶 traffic（consume-rate）；cpu 移除（唯一消費者是 hub）回歸權威 |
| 6 | `interruptible`（可中斷性） | [axis_6_interruptible.md](axis_6_interruptible.md) | ✅ 定案（R3，序→名目）：`unsigned level`(**名目分類碼·禁大小比較**，0=unsafe/1=safe/…/5=graceful,≥6 自定義；zero-init=unsafe)；**extra 上收 `Meta::extra`（2026-06-28，保留 struct 與具名常數）** |
| 7 | `guarantee`（執行保證） | [axis_7_guarantee.md](axis_7_guarantee.md) | ✅ 定案（R1）：封閉 `enum class Guarantee : unsigned`（none=0 預設/idempotent/transactional）；**已內聯成 Meta 裸欄位、移除 GuaranteeField、extra 上收 Meta（2026-06-28）**；與軸 6 開放碼表相反（值集封閉故用 enum） |
| 8 | `dry_run`（乾跑） | [axis_8_dry_run.md](axis_8_dry_run.md) | ✅ 定案：描述層只留 `bool allow_dry_run = false`（同軸 3 純 bool）；object 細節 flag/state_entry/error_entry 全降級到實作層待設計 |
| 9 | `nondeterministic`（確定性/治理證書） | [axis_9_nondeterministic.md](axis_9_nondeterministic.md) | ✅ 定案（R1）：單一 `unsigned uncertainty = 0`（0=完全確定；愈高愈不確定；馴化使其下降）；**已內聯成 Meta 裸欄位、證書（model/test_set/stability）改走單一 `Meta::extra`（2026-06-28）**。權威三態摺成單調量尺 |

## 跨軸的全域立場 / 待決議題（隨討論累積）

- ~~**序列化**：demo 要不要做 `--metadata` JSON？~~ → **已定（2026-06-24）：要，且是 defs 層核心職責**（見上「lib 是什麼」）。
- **★ 跨軸型別強制：放棄，退文件約束（2026-06-27 defs 重新思考定）。**
  defs 層**不做**跨軸 cross-field 驗證。跨軸無意義/非法組合——`stateless × transactional`(3×7)、
  `one_shot × idle`(2×5)、`stateless × state_dirs`(3×4)、`uncertainty × 證書`(9 內) 等——一律**文件約束**，
  不靠型別 unrepresentable。理由：跨軸強制要嘛把獨立軸綁死成巢狀型別（毀掉單軸乾淨）、要嘛得另立
  「總 metadata cross-field 驗證器」（過度工程）。**「讓非法狀態無法被表達」只在單軸內貫徹，跨軸明文放棄。**
  → 一次收口 defs_review 索引「跨軸主題 2」的所有實例。
- **★ 「別管 hub」原則（2026-06-27 使用者定）。** 逐軸 defs 只按「描述本身」定形，不讓 hub 消費題當尺。
  純 hub 動機的補欄/結構化一律不採（已據此：軸 1 砍 access、軸 5 砍 cpu、軸 6 拒 optional、軸 7 冪等鍵留 extra）。
  非核心描述丟 extra 了事。
- **★ brownfield 模型：greenfield-wrap（2026-06-27 使用者選「前」）。**
  **ac_helper = greenfield 創作 lib，既有 CLI（git/rsync/terraform）一律經 wrapper 進系統。**
  有 `--metadata` 的實體永遠是 wrapper（greenfield ac_helper 程式），不是被包的 CLI；既有程式的特殊 flag/介面
  （git `-n`、rsync `--dry-run`）是 **wrapper 的 impl 細節，不進 defs**。
  → 一次收掉 defs_review 索引「主題 4 brownfield 盲點」：軸 8 維持 bool、軸 1 不為 brownfield 保留傳輸/介面欄位。
