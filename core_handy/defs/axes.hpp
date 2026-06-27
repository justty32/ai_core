// defs/axes.hpp — 九軸描述性 metadata 型別（header-only）
//
// 對應 notes/ 各軸的逐軸定案（2026-06-27 defs 重新思考收尾）。
// 這層只放「描述一個函數是什麼」的純資料型別；「替程式做事」的設施碼歸 impl/。
//
// 軸 4 state_dirs 不在此層——它是純設施，描述面外包給軸 3 stateful。
// 全域立場（見 notes/00_index.md）：別管 hub / 跨軸只用文件約束 / brownfield 經 wrapper。
#pragma once

#include <map>
#include <optional>
#include <string>

namespace ac {

// 全軸共用的逃生艙型別：低保真 string→string，刻意製造升級壓力。
using Extra = std::optional<std::map<std::string, std::string>>;

// ── 軸 1 entries（I/O 出入口）─────────────────────────────────────────
// Round 13：兩碼表 + extra。砍掉 access（冗餘於軸 3 + 傳輸身分）。
// direction/content 為開放整數碼表，序列化直出整數。mutation/transport/
// flow（流動模式）一律 PARKED → extra。
struct Entry {
  static constexpr unsigned in = 0, out = 1, in_out = 2;  // direction 碼
  static constexpr unsigned binary = 0, text = 1;         // content 碼（≥2 擴充：json/status-int…）

  unsigned direction = 0;   // 0=in / 1=out / 2=in_out
  unsigned content   = 0;   // 0=binary / 1=text / ≥2 擴充
  Extra    extra;           // transport / mode / mutation 等 PARKED 暫存
};

// 具名通道 → entry。空 map ＝ 預設單一 stdio entry。
using Entries = std::map<std::string, Entry>;

// ── 軸 2 lifecycle（生命週期）─────────────────────────────────────────
// Round 4：bool persistent + extra。流動模式不在此（歸軸 1）。
struct Lifecycle {
  bool  persistent = false;   // false=one_shot(預設·自我終止) / true=persistent(顯式終止)
  Extra extra;                // 常駐子型(lazy/warm/eager/periodic) / detached 等變體描述
};

// ── 軸 3 state（跨呼叫狀態）───────────────────────────────────────────
// Round 2：純 bool，無 extra。內聯進 Meta（見下），不獨立成 struct。
//   false=stateless(預設) / true=stateful_external（碰外部狀態，讀寫都算）

// ── 軸 5 resources（資源特性）─────────────────────────────────────────
// Round 3：無固定欄位、opt 預定義 key + extra。砍掉 cpu（唯一消費者是 hub）。
// 容量/時間值先當 string（不正規化）；network.traffic 餵 consume-rate 計量。
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
  Extra                      extra;       // 自定義依賴（llm_entry/db/render_server…）；cpu 改走這
};

// ── 軸 6 interruptible（可中斷性）─────────────────────────────────────
// Round 3：unsigned level（名目分類碼·禁大小比較，像 errno）+ extra。
// 撤掉「有序階梯」框架（safe(1)/graceful(5) 在能否直接 kill 上幾乎相反）。
struct Interruptible {
  static constexpr unsigned unsafe = 0, safe = 1, resettable = 2,
                            rollback = 3, resumable = 4, graceful = 5;  // ≥6 自定義

  unsigned level = 0;   // 名目分類碼；0=unsafe(zero-init 保守預設)；禁大小比較
  Extra    extra;       // condition / reset_hint / 正交補充（如同時 graceful+resumable）
};

// ── 軸 7 guarantee（執行保證）─────────────────────────────────────────
// Round 1：封閉 enum（值集封閉故用 enum，讓非法值無法被表達）+ extra。
enum class Guarantee : unsigned {
  none          = 0,   // 無承諾（預設·zero-init）
  idempotent    = 1,   // 重複執行 ≡ 執行一次；中斷後安全重試
  transactional = 2,   // 全成功或全不發生（ACID）；中途失敗自動回滾
};

struct GuaranteeField {
  Guarantee guarantee = Guarantee::none;
  Extra     extra;     // 回滾機制 hint / 冪等鍵帶法（hub-only，故留 extra）
};

// ── 軸 8 dry_run（乾跑）───────────────────────────────────────────────
// Round 2：純 bool，無 extra。brownfield flag（git -n…）歸 wrapper impl。
//   false=不支援(預設) / true=支援乾跑。內聯進 Meta（見下）。

// ── 軸 9 nondeterministic（確定性 / 治理證書）─────────────────────────
// Round 1（B）：unsigned uncertainty（債務儀表·telos→0）+ extra（證書）。
struct Nondeterministic {
  unsigned uncertainty = 0;   // 0=完全確定(預設)；愈高愈不確定；馴化使其下降
  Extra    extra;             // 治理證書：model / test_set / stability + 自訂
};

// ── 總 metadata：組合九軸（軸 4 純設施不入）───────────────────────────
// 軸 3 / 軸 8 是純 bool，內聯；其餘各帶自己的 extra 結構。
struct Meta {
  Entries          entries;                 // 軸 1（空＝預設 stdio）
  Lifecycle        lifecycle;               // 軸 2
  bool             stateful = false;        // 軸 3
  Resources        resources;               // 軸 5
  Interruptible    interruptible;           // 軸 6
  GuaranteeField   guarantee;               // 軸 7
  bool             allow_dry_run = false;   // 軸 8
  Nondeterministic nondeterministic;        // 軸 9
};

}  // namespace ac
