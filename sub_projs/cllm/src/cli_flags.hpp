#pragma once
// cli_flags.hpp — 反射 llm::abi::Client 的欄位 → CLI 連線/取樣旗標與 --help 用法。
//
// 「不搬 schema 表、全靠 struct 反射」的落地：要多一個旋鈕＝在 Client 加一個欄位，旗標與 --help
// 自動長出來、零改動。模板部分（型別分派）須在標頭給 run() 的 coerce 迴圈用；非模板實作在 .cpp。

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace cli::flags {

// 一個反射出來的旗標：命令列旗標、對應的 Client 欄位名、型別提示。
struct FlagInfo {
  std::string flag, field, hint;
};

// 欄位名（api_key）→ 旗標（--api-key）：底線換連字號。
std::string kebab_flag(std::string_view name);

// 反射 llm::abi::Client → 每欄一個 FlagInfo（解析用的「合法旗標表」與 --help 用法共用這份）。
std::vector<FlagInfo> client_flags();

// 印用法到 stderr（固定旗標＋反射旗標＋數值範圍/範例標註＋設定來源說明）。
void print_usage();

// ── 型別小工具（模板，須在標頭）：認 std::optional、取其 value_type ──
template <class> constexpr bool is_optional_v = false;
template <class U> constexpr bool is_optional_v<std::optional<U>> = true;
template <class T> struct value_type_of { using type = T; };
template <class U> struct value_type_of<std::optional<U>> { using type = U; };
template <class T> using value_type_of_t = typename value_type_of<T>::type;

// 給 --help 的型別提示（從欄位型別反射出來，非手寫）。
template <class T> const char *type_hint() {
  using V = value_type_of_t<T>;
  const bool opt = is_optional_v<T>;
  if constexpr (std::is_same_v<V, std::string>)
    return opt ? "字串（選填）" : "字串";
  else if constexpr (std::is_same_v<V, float>)
    return "小數";
  else if constexpr (std::is_same_v<V, int>)
    return "整數";
  else if constexpr (std::is_same_v<V, long>)
    return "整數（毫秒）";
  else
    return "?";
}

// 把命令列字串 raw 轉成純量 U（string/float/int/long），型別不合就 throw（帶旗標名）。
template <class U> U parse_scalar(const std::string &flag, const std::string &raw) {
  if constexpr (std::is_same_v<U, std::string>) {
    return raw;
  } else {
    try {
      if constexpr (std::is_same_v<U, float>)
        return std::stof(raw);
      else if constexpr (std::is_same_v<U, int>)
        return std::stoi(raw);
      else if constexpr (std::is_same_v<U, long>)
        return std::stol(raw);
    } catch (const std::exception &) {
      throw std::runtime_error(flag + " 需要數值，得到：" + raw);
    }
    throw std::runtime_error(flag + "：不支援的欄位型別"); // 抵達不了；擺著讓非 void 分支完整
  }
}

// 把 raw 塞進反射拿到的欄位 field（optional 就填其 value_type 並包起來）。
template <class F> void assign_field(F &field, const std::string &flag, const std::string &raw) {
  using T = std::remove_reference_t<F>;
  if constexpr (is_optional_v<T>)
    field = parse_scalar<value_type_of_t<T>>(flag, raw);
  else
    field = parse_scalar<T>(flag, raw);
}

} // namespace cli::flags
