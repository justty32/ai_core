// defs/axes.hpp — 九軸描述性 metadata 型別（header-only）
//
// 對應 notes/ 各軸的逐軸定案（2026-06-27 defs 重新思考收尾）。
// 這層只放「描述一個函數是什麼」的純資料型別；「替程式做事」的設施碼歸 impl/。
//
// 軸 4 state_dirs 不在此層——它是純設施，描述面外包給軸 3 stateful。
// 全域立場（見 notes/00_index.md）：別管 hub / 跨軸只用文件約束 / brownfield 經 wrapper。
//
// ★ 2026-06-28 決定：
//   (1) extra 全軸收斂成單一 `Meta::extra`；各軸不再各帶 extra。
//   (2) 單欄位軸（2 lifecycle / 7 guarantee / 9 nondeterministic）直接內聯進 Meta 成裸欄位，
//       與軸 3 stateful / 軸 8 allow_dry_run 同形，不再包 struct。
#pragma once

#include <map>
#include <optional>
#include <string>

namespace ac {

// 全軸共用的逃生艙型別：低保真 string→string，刻意製造升級壓力。
// 自 2026-06-28 起只用於 `Meta::extra`（單一袋，見檔尾）。
using Extra = std::optional<std::map<std::string, std::string>>;

// ── 軸 1 entries（I/O 出入口）─────────────────────────────────────────
// Round 14（Unix 統一 I/O）：三碼表 direction / flow / content。
// 「一切都是檔案」：transport 種類不進描述——退化成 runtime 的位址 scheme
// （`-`=stdio / path / `tcp://…` / `shm:…`），作者與 hub 一律用同一對 read/write。
// 描述只保留 read/write 統一介面藏不掉的 flow（batch vs streaming）。
// mutation → 軸 3 stateful；transport 身分/位址 → runtime（impl `read_all`/`resolve`）。
struct Entry {
  static constexpr unsigned in = 0, out = 1, in_out = 2;  // direction 碼
  static constexpr unsigned batch = 0, streaming = 1;     // flow 碼（≥2 擴充：interactive/duplex）
  static constexpr unsigned binary = 0, text = 1;         // content 碼（≥2 擴充：json/status-int…）

  unsigned direction = 0;   // 0=in / 1=out / 2=in_out
  unsigned flow      = 0;   // 0=batch（整塊·可 seek）/ 1=streaming（逐塊·順序·可能不終止）/ ≥2 擴充
  unsigned content   = 0;   // 0=binary / 1=text / ≥2 擴充
};

// 具名通道 → entry。空 map ＝ 預設單一 stdio entry。
using Entries = std::map<std::string, Entry>;

// ── 軸 2 lifecycle（生命週期）─────────────────────────────────────────
// Round 4：bool persistent，內聯進 Meta（見下）。流動模式不在此（歸軸 1）。
//   false=one_shot(預設·自我終止) / true=persistent(顯式終止)。
//   常駐子型(lazy/warm/eager/periodic) / detached 等變體描述 → Meta::extra。

// ── 軸 3 state（跨呼叫狀態）───────────────────────────────────────────
// Round 2：純 bool，內聯進 Meta（見下），不獨立成 struct。
//   false=stateless(預設) / true=stateful_external（碰外部狀態，讀寫都算）

// ── 軸 5 resources（資源特性）─────────────────────────────────────────
// Round 3：無固定欄位、opt 預定義 key。砍掉 cpu（唯一消費者是 hub）。
// 容量/時間值先當 string（不正規化）；network.traffic 餵 consume-rate 計量。
// 自定義依賴（llm_entry/db/render_server…）與 cpu → Meta::extra。
struct Memory  { std::optional<std::string> startup, peak, idle; };  // "4gb"＝只給 peak；idle 僅 persistent
struct Gpu     { std::optional<std::string> vram; };                 // 有值＝需 GPU
struct Time    { std::optional<std::string> expected, max; };
struct Network { std::optional<std::string> traffic; };              // 有值＝需網路；traffic＝流量計入

struct Resources {
  std::optional<Memory>      memory;
  std::optional<Gpu>         gpu;
  std::optional<Time>        time;
  std::optional<std::string> disk;       // "500mb"（執行期暫用）
  std::optional<Network>     network;
};

// ── 軸 6 interruptible（可中斷性）─────────────────────────────────────
// Round 3：unsigned level（名目分類碼·禁大小比較，像 errno）。
// condition / reset_hint / 正交補充（如同時 graceful+resumable） → Meta::extra。
struct Interruptible {
  static constexpr unsigned unsafe = 0, safe = 1, resettable = 2,
                            rollback = 3, resumable = 4, graceful = 5;  // ≥6 自定義

  unsigned level = 0;   // 名目分類碼；0=unsafe(zero-init 保守預設)；禁大小比較
};

// ── 軸 7 guarantee（執行保證）─────────────────────────────────────────
// Round 1：封閉 enum（值集封閉故用 enum，讓非法值無法被表達），內聯進 Meta（見下）。
// 回滾機制 hint / 冪等鍵帶法 → Meta::extra。
enum class Guarantee : unsigned {
  none          = 0,   // 無承諾（預設·zero-init）
  idempotent    = 1,   // 重複執行 ≡ 執行一次；中斷後安全重試
  transactional = 2,   // 全成功或全不發生（ACID）；中途失敗自動回滾
};

// ── 軸 8 dry_run（乾跑）───────────────────────────────────────────────
// Round 2：純 bool。brownfield flag（git -n…）歸 wrapper impl。
//   false=不支援(預設) / true=支援乾跑。內聯進 Meta（見下）。

// ── 軸 9 nondeterministic（確定性 / 治理證書）─────────────────────────
// Round 1（B）：unsigned uncertainty（債務儀表·telos→0），內聯進 Meta（見下）。
// 治理證書（model / test_set / stability + 自訂） → Meta::extra。

// ── 總 metadata：組合九軸（軸 4 純設施不入）───────────────────────────
// 單欄位軸（2/3/7/8/9）全部內聯為裸欄位；軸 1/5/6 仍帶各自結構（但無 extra）。
// 2026-06-28：全軸 extra 收斂為下方唯一一個 `extra` 袋。
struct Meta {
  Entries       entries;                       // 軸 1（空＝預設 stdio）
  bool          persistent = false;            // 軸 2（false=one_shot / true=persistent）
  bool          stateful = false;              // 軸 3（false=stateless / true=stateful_external）
  Resources     resources;                     // 軸 5
  Interruptible interruptible;                 // 軸 6
  Guarantee     guarantee = Guarantee::none;   // 軸 7
  bool          allow_dry_run = false;         // 軸 8
  unsigned      uncertainty = 0;               // 軸 9（債務儀表，0=完全確定）
  Extra         extra;                         // ★ 全軸共用的單一逃生艙（原各軸 extra 統一於此）
};

}  // namespace ac
