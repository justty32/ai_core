// cli.cpp — cli.hpp 的實作：反射驅動旗標 ＋ config 檔 ＋ 走 cabi.hpp 發問的 unix filter。
//
// 流程：
//   (1) client_flags()：反射 llm::abi::Client 的欄位 → 每欄一個 {旗標, 欄位名, 型別提示}。
//       同一份餵「合法旗標表」與「--help 用法」——用法也是從 struct 生的（沿用舊 cli 作法）。
//   (2) 解析 argv：固定旗標（--stream/--image/--schema/--config/--help）特判；反射旗標吃下一個
//       argv 收進 raw_values[欄位名]；不帶 `-` 的當位置參數（拼成 prompt）。未知旗標／缺值即報錯。
//   (3) 組 Client：Client{} 預設 → 載 config 檔（glaze 反射整份覆寫）→ 反射把 raw_values coerce 覆寫。
//   (4) 組 Request（prompt＋stream＋--schema 檔＋--image 檔）→ client.ask()，output 走 handlers。
//
// 為什麼配 idx++ 就能拿到欄位名：glz::for_each_field 用 fold-over-逗號運算子依序呼叫 callable
//   （由左到右求值），故第 k 次收到的欄位對應 reflect::keys[k]（同舊 cli）。

#include "cli.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <glaze/glaze.hpp> // reflect<T>::keys / for_each_field / read_json
#include "cabi.hpp"        // C++ 薄鏡像：llm::abi::Client / Request / Handlers / Context / Status

namespace cli {
namespace {

// ── 退出碼（見 cli.hpp 檔頭）─────────────────────────────────────────────────
constexpr int kExitOk = 0;      // 成功
constexpr int kExitUsage = 1;   // 用法錯：未知旗標／缺 prompt／檔案讀不到／config 壞
constexpr int kExitRequest = 2; // 請求失敗：傳輸／後端
constexpr int kExitCancel = 130;// SIGINT 取消（128+SIGINT）

// ★ config 檔路徑的環境變數：只「指路」、不存設定值。前綴取執行檔名 llm、加 _CLI_ 收窄以避免撞
//   泛用的 LLM_（見 cli.hpp）。要改名只動這一行＋default_config_path()。
constexpr const char *kConfigEnvVar = "LLM_CLI_CONFIG";

// ── 型別小工具：認 std::optional、取其 value_type（同舊 cli）───────────────────
template <class> constexpr bool is_optional_v = false;
template <class U> constexpr bool is_optional_v<std::optional<U>> = true;
template <class T> struct value_type_of { using type = T; };
template <class U> struct value_type_of<std::optional<U>> { using type = U; };
template <class T> using value_type_of_t = typename value_type_of<T>::type;

// 欄位名（api_key）→ 旗標（--api-key）：底線換連字號。
std::string kebab_flag(std::string_view name) {
  std::string s = "--";
  for (char c : name)
    s += (c == '_') ? '-' : c;
  return s;
}

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

// 一個反射出來的旗標：命令列旗標、對應的 Client 欄位名、型別提示。
struct FlagInfo {
  std::string flag, field, hint;
};

// ★ 反射 llm::abi::Client 的欄位 → 每欄一個 FlagInfo。空 probe 走 for_each_field，配 idx++
//   對到 reflect::keys[idx]（順序有保證，見檔頭）。解析與 --help 都吃這份。
std::vector<FlagInfo> client_flags() {
  std::vector<FlagInfo> out;
  llm::abi::Client probe{};
  std::size_t idx = 0;
  glz::for_each_field(probe, [&](auto &&f) {
    using T = std::remove_cvref_t<decltype(f)>;
    std::string_view name = glz::reflect<llm::abi::Client>::keys[idx++];
    out.push_back(FlagInfo{kebab_flag(name), std::string(name), type_hint<T>()});
  });
  return out;
}

void print_usage() {
  std::fprintf(stderr,
      "用法：llm [旗標...] [prompt...]        # 沒給 prompt 就從 stdin 整段讀\n"
      "  範例：llm 用一句話介紹你自己\n"
      "        echo \"數到五\" | llm --stream\n"
      "        llm 這張圖是什麼 --image ./cat.jpg\n"
      "\n固定旗標：\n"
      "  --stream               串流逐段吐 stdout（布林，無值）\n"
      "  --image <檔>           夾帶輸入媒體（可重複；mime 由副檔名推）\n"
      "  --schema <檔>          JSON Schema 檔，要求結構化輸出\n"
      "  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）\n"
      "  --help, -h             顯示本說明\n"
      "\n連線／取樣旗標（由 llm::abi::Client 欄位反射生成，未給即不送、交後端默認）：\n");
  for (const auto &fi : client_flags())
    std::fprintf(stderr, "  %-22s %s\n", fi.flag.c_str(), fi.hint.c_str());
  std::fprintf(stderr,
      "\n設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。\n"
      "config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。\n"
      "  （env 只用來指定 config 檔路徑，不存任何設定值。）\n"
      "離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。\n",
      kConfigEnvVar);
}

// 讀整個檔到 out（二進位）。失敗回 false 並把原因寫進 err。
bool read_file(const std::string &path, std::string &out, std::string &err) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    err = "讀不到檔案：" + path;
    return false;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

// 由副檔名猜 mime（夠 CLI 用；認不得就退成 octet-stream，交後端／data URI 自處理）。
std::string mime_of(const std::string &path) {
  auto dot = path.find_last_of('.');
  std::string ext = (dot == std::string::npos) ? "" : path.substr(dot + 1);
  for (char &c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif") return "image/gif";
  if (ext == "webp") return "image/webp";
  if (ext == "wav") return "audio/wav";
  if (ext == "mp3") return "audio/mpeg";
  return "application/octet-stream";
}

// ~/.config/llm/config.json 的家目錄探測（沒 HOME 就回空）。
std::string default_config_path() {
  const char *home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return std::string(home) + "/.config/llm/config.json";
}

// ── SIGINT → 取消在途請求（POSIX）。handler 只做 atomic 存（見 Context::cancel），async-signal 安全。
std::atomic<llm::abi::Context *> g_ctx{nullptr};
#ifndef _WIN32
extern "C" void on_sigint(int) {
  if (auto *c = g_ctx.load())
    c->cancel();
}
#endif

} // namespace

int run(const std::vector<std::string> &args) {
  // 合法反射旗標表：--flag → Client 欄位名
  std::map<std::string, std::string> flag_to_field;
  for (const auto &fi : client_flags())
    flag_to_field[fi.flag] = fi.field;

  // ── (2) 解析 argv（args[0]＝執行檔，從 1 起）──
  std::map<std::string, std::string> raw_values; // 反射欄位名 → 原始字串
  std::vector<std::string> prompt_parts;         // 位置參數 → 拼成 prompt
  std::vector<std::string> image_paths;          // --image（可重複）
  std::string schema_path, config_path;
  bool has_schema = false, has_config = false, stream = false;

  // 需要吃下一個 argv 的取值小工具：缺值即報錯。
  auto need_value = [&](std::size_t &i, const std::string &flag, std::string &dst) -> bool {
    if (i + 1 >= args.size()) {
      std::fprintf(stderr, "%s 缺少值\n", flag.c_str());
      return false;
    }
    dst = args[++i];
    return true;
  };

  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string &a = args[i];
    if (a == "--help" || a == "-h") {
      print_usage();
      return kExitOk;
    }
    if (a == "--stream") {
      stream = true;
      continue;
    }
    if (a == "--image") {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      image_paths.push_back(v);
      continue;
    }
    if (a == "--schema") {
      if (!need_value(i, a, schema_path))
        return kExitUsage;
      has_schema = true;
      continue;
    }
    if (a == "--config") {
      if (!need_value(i, a, config_path))
        return kExitUsage;
      has_config = true;
      continue;
    }
    if (auto it = flag_to_field.find(a); it != flag_to_field.end()) {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      raw_values[it->second] = v;
      continue;
    }
    if (a.size() >= 1 && a[0] == '-' && a != "-") { // 「-」單獨當位置參數（stdin 慣例），其餘 - 開頭＝未知旗標
      std::fprintf(stderr, "未知旗標：%s\n", a.c_str());
      print_usage();
      return kExitUsage;
    }
    prompt_parts.push_back(a); // 位置參數
  }

  // ── prompt：有位置參數就拼（空格）；否則從 stdin 整段讀 ──
  std::string prompt;
  if (!prompt_parts.empty()) {
    for (std::size_t k = 0; k < prompt_parts.size(); ++k) {
      if (k)
        prompt += ' ';
      prompt += prompt_parts[k];
    }
  } else {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    prompt = ss.str();
    while (!prompt.empty() && (prompt.back() == '\n' || prompt.back() == '\r'))
      prompt.pop_back(); // 去掉尾端換行，避免多餘空白進 prompt
  }
  if (prompt.empty()) {
    std::fprintf(stderr, "缺少 prompt：給位置參數或從 stdin 餵入\n");
    print_usage();
    return kExitUsage;
  }

  // ── (3) 組 Client：預設 → config 檔（glaze 反射整份覆寫）→ 反射旗標 coerce 覆寫 ──
  llm::abi::Client client;

  // config 檔路徑：--config ＞ env GALTXT_LLM_CONFIG ＞ ~/.config/galtxt/llm.json。
  //   前二者是「明指」→ 讀不到就報錯；後者是「探測」→ 不存在就靜默略過（但存在卻壞＝報錯）。
  std::string cfg_path;
  bool cfg_named = false; // 明指（flag/env）＝必須成功載入
  if (has_config) {
    cfg_path = config_path;
    cfg_named = true;
  } else if (const char *env = std::getenv(kConfigEnvVar); env && *env) {
    cfg_path = env;
    cfg_named = true;
  } else {
    cfg_path = default_config_path();
  }
  if (!cfg_path.empty()) {
    std::string body, err;
    if (read_file(cfg_path, body, err)) {
      if (auto ec = glz::read_json(client, body)) {
        std::fprintf(stderr, "config JSON 解析失敗（%s）：%s\n", cfg_path.c_str(),
                     glz::format_error(ec, body).c_str());
        return kExitUsage;
      }
    } else if (cfg_named) {
      std::fprintf(stderr, "%s\n", err.c_str()); // 明指卻讀不到＝用法錯
      return kExitUsage;
    }
    // 探測路徑讀不到＝沒設定檔，靜默用預設
  }

  // 反射把 raw_values coerce 進對應欄位（覆寫 config）
  bool coerce_err = false;
  std::size_t idx = 0;
  glz::for_each_field(client, [&](auto &&f) {
    std::string key(glz::reflect<llm::abi::Client>::keys[idx++]);
    auto it = raw_values.find(key);
    if (it == raw_values.end())
      return;
    try {
      assign_field(f, kebab_flag(key), it->second);
    } catch (const std::exception &e) {
      std::fprintf(stderr, "%s\n", e.what());
      coerce_err = true;
    }
  });
  if (coerce_err)
    return kExitUsage;

  // ── (4) 組 Request（prompt＋stream＋schema 檔＋image 檔）──
  llm::abi::Request req;
  req.prompt = prompt;
  req.stream = stream;
  if (has_schema) {
    std::string body, err;
    if (!read_file(schema_path, body, err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return kExitUsage;
    }
    req.schema = llm::abi::Schema{.name = "response", .json = body};
  }
  for (const auto &p : image_paths) {
    std::string bytes, err;
    if (!read_file(p, bytes, err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return kExitUsage;
    }
    req.media.push_back(llm::abi::MediaIn{.url = "", .mime = mime_of(p), .bytes = std::move(bytes)});
  }

  // ── 出口 handlers：文字吐 stdout（串流逐段即時 flush；非串流一次整段）；錯誤吐 stderr ──
  bool wrote_any = false;
  llm::abi::Handlers h;
  h.on_text = [&](std::string_view sv) {
    std::fwrite(sv.data(), 1, sv.size(), stdout);
    std::fflush(stdout);
    wrote_any = true;
    return false; // 不中止
  };
  h.on_error = [&](std::string_view msg) {
    std::fprintf(stderr, "請求失敗：%.*s\n", static_cast<int>(msg.size()), msg.data());
  };
  // on_tool/on_media 不設：CLI 不開 tools／modalities，這兩路不會觸發。

  // ── 發問（可從 SIGINT 取消）──
  llm::abi::Context ctx;
  g_ctx.store(&ctx);
#ifndef _WIN32
  std::signal(SIGINT, on_sigint);
#endif
  llm::abi::Status st = client.ask(req, h, &ctx);
  g_ctx.store(nullptr);

  if (wrote_any && st == llm::abi::Status::Ok)
    std::fputc('\n', stdout); // 補尾端換行（stdout 收尾乾淨）

  switch (st) {
  case llm::abi::Status::Ok:
    return kExitOk;
  case llm::abi::Status::Cancelled:
    std::fprintf(stderr, "\n已取消\n");
    return kExitCancel;
  default: // Error：on_error 已印診斷
    return kExitRequest;
  }
}

} // namespace cli
