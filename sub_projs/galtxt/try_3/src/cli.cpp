// cli.cpp — cli.hpp 的實作：反射驅動的 --flag 解析器。
//
// 三個階段，兩次反射：
//   (1) client_flags()：反射 llm::Client 的欄位 → 每欄一個 {flag, 欄位名, 型別提示}。
//       這一份同時餵給「合法旗標表」與「--help 用法」——用法也是從 struct 生的。
//   (2) 解析 argv：認得的旗標吃下一個 argv 當原始值，收進 raw_values[欄位名]＝字串；
//       --prompt/--stream/--help 是特例（非 Client 欄位）。未知旗標／缺值即報錯。
//   (3) 再反射一次 llm::Client 的欄位，把 raw_values 裡的字串按「欄位型別」coerce 進去。
//       型別分派用 if constexpr：string / optional<string|float|int> / long。
//
// 為什麼配 idx++ 就能拿到欄位名：glz::for_each_field 用 fold-over-逗號運算子依序呼叫 callable
//   （逗號運算子保證由左到右求值），故 callable 第 k 次收到的欄位對應 reflect::keys[k]。

#include "cli.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <glaze/glaze.hpp>   // reflect<T>::keys / for_each_field
#include "llm.hpp"           // 反射的對象＝llm::Client；from_env() 當 base

namespace cli {
namespace {

// ── 型別小工具：認 std::optional、取其 value_type ──────────────────────────
template <class>       constexpr bool is_optional_v = false;
template <class U>     constexpr bool is_optional_v<std::optional<U>> = true;
template <class T>     struct value_type_of                 { using type = T; };
template <class U>     struct value_type_of<std::optional<U>> { using type = U; };
template <class T>     using  value_type_of_t = typename value_type_of<T>::type;

// 欄位名（api_key）→ 旗標（--api-key）：底線換連字號（對齊 try_2 cli.lua 的 flag_of）。
std::string kebab_flag(std::string_view name) {
    std::string s = "--";
    for (char c : name) s += (c == '_') ? '-' : c;
    return s;
}

// 給 --help 的型別提示（也是從欄位型別反射出來的，非手寫）。
template <class T>
const char* type_hint() {
    using V = value_type_of_t<T>;
    const bool opt = is_optional_v<T>;
    if constexpr (std::is_same_v<V, std::string>) return opt ? "字串（選填）" : "字串";
    else if constexpr (std::is_same_v<V, float>)  return "小數";
    else if constexpr (std::is_same_v<V, int>)    return "整數";
    else if constexpr (std::is_same_v<V, long>)   return "整數（毫秒）";
    else return "?";
}

// 把命令列字串 raw 轉成純量 U（string/float/int/long），型別不合就 throw（帶旗標名的訊息）。
template <class U>
U parse_scalar(const std::string& flag, const std::string& raw) {
    if constexpr (std::is_same_v<U, std::string>) {
        return raw;
    } else {
        try {
            if constexpr (std::is_same_v<U, float>) return std::stof(raw);
            else if constexpr (std::is_same_v<U, int>) return std::stoi(raw);
            else if constexpr (std::is_same_v<U, long>) return std::stol(raw);
        } catch (const std::exception&) {
            throw std::runtime_error(flag + " 需要數值，得到：" + raw);
        }
        // 上面所有分支都 return，抵達不了這裡；擺著讓非 void 分支完整。
        throw std::runtime_error(flag + "：不支援的欄位型別");
    }
}

// 把 raw 塞進反射拿到的欄位 field（optional 就填其 value_type 並包起來）。
template <class F>
void assign_field(F& field, const std::string& flag, const std::string& raw) {
    using T = std::remove_reference_t<F>;
    if constexpr (is_optional_v<T>) {
        field = parse_scalar<value_type_of_t<T>>(flag, raw);
    } else {
        field = parse_scalar<T>(flag, raw);
    }
}

// 一個反射出來的旗標：命令列旗標、對應的 Client 欄位名、型別提示。
struct FlagInfo {
    std::string flag;
    std::string field;
    std::string hint;
};

// ★ 反射 llm::Client 的欄位 → 每欄一個 FlagInfo。用一個空 probe 走 for_each_field，
//   配 idx++ 對到 reflect::keys[idx]（順序有保證，見檔頭）。解析與用法都吃這份。
std::vector<FlagInfo> client_flags() {
    std::vector<FlagInfo> out;
    llm::Client probe{};
    std::size_t idx = 0;
    glz::for_each_field(probe, [&](auto&& f) {
        using T = std::remove_cvref_t<decltype(f)>;
        std::string_view name = glz::reflect<llm::Client>::keys[idx++];
        out.push_back(FlagInfo{ kebab_flag(name), std::string(name), type_hint<T>() });
    });
    return out;
}

void print_usage() {
    std::fprintf(stderr,
        "用法：try3.exe --prompt <文字> [--旗標 值 ...]\n"
        "  --prompt <文字>    要問的內容（必填）\n"
        "  --stream           串流逐段印出（布林旗標，無值）\n"
        "  --help, -h         顯示本說明\n"
        "取樣／連線旗標（由 llm::Client 欄位反射生成，未給即不送、交後端默認）：\n");
    for (const auto& fi : client_flags())
        std::fprintf(stderr, "  %-20s %s\n", fi.flag.c_str(), fi.hint.c_str());
    std::fprintf(stderr,
        "\n備註：base 走 llm::from_env()（預設打本機 LM Studio）；\n"
        "      --endpoint file://<絕對路徑> 可指向離線 fixture 測 CLI 本身。\n");
}

}  // namespace

int run(const std::vector<std::string>& args) {
    // 合法旗標表：--flag → Client 欄位名（反射生成）
    std::map<std::string, std::string> flag_to_field;
    for (const auto& fi : client_flags()) flag_to_field[fi.flag] = fi.field;

    // ── (2) 解析 argv（args[0]＝執行檔，從 1 起）──
    std::map<std::string, std::string> raw_values;   // 欄位名 → 原始字串
    std::string prompt;
    bool prompt_given = false;
    bool stream = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--help" || a == "-h") { print_usage(); return 0; }
        if (a == "--stream") { stream = true; continue; }
        if (a == "--prompt") {
            if (i + 1 >= args.size()) { std::fprintf(stderr, "--prompt 缺少值\n"); return 1; }
            prompt = args[++i];
            prompt_given = true;
            continue;
        }
        auto it = flag_to_field.find(a);
        if (it == flag_to_field.end()) {
            std::fprintf(stderr, "未知旗標：%s\n", a.c_str());
            print_usage();
            return 1;
        }
        if (i + 1 >= args.size()) { std::fprintf(stderr, "%s 缺少值\n", a.c_str()); return 1; }
        raw_values[it->second] = args[++i];
    }

    if (!prompt_given || prompt.empty()) {
        std::fprintf(stderr, "缺少 --prompt\n");
        print_usage();
        return 1;
    }

    // ── (3) base＝from_env()，反射把 raw_values coerce 進對應欄位（覆寫預設）──
    llm::Client client = llm::from_env();
    bool coerce_err = false;
    std::size_t idx = 0;
    glz::for_each_field(client, [&](auto&& f) {
        std::string key(glz::reflect<llm::Client>::keys[idx++]);
        auto it = raw_values.find(key);
        if (it == raw_values.end()) return;
        try {
            assign_field(f, kebab_flag(key), it->second);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
            coerce_err = true;
        }
    });
    if (coerce_err) return 1;

    // ── 發問：串流注入 stdout callback（比照 try_2 cli.lua）；非串流一次印出 ──
    try {
        if (stream) {
            client.ask(prompt, true, [](std::string_view piece) {
                std::printf("%.*s", static_cast<int>(piece.size()), piece.data());
                std::fflush(stdout);   // 逐段即時吐出，不等緩衝
                return true;
            });
            std::printf("\n");
        } else {
            std::string ans = client.ask(prompt);
            std::printf("%s\n", ans.c_str());
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "請求失敗：%s\n", e.what());
        return 2;
    }
    return 0;
}

}  // namespace cli
