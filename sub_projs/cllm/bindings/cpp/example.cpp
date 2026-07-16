// example.cpp — cllm C++ 便利層（llm.hpp + llm_reflect.hpp）＋ shell(CLI) 呼叫。
// 編：g++ -std=c++23 example.cpp $(pkg-config --cflags --libs cllm) -o example
// 跑：source ~/repo/dev/env.sh 後  ./example "$CLLM_FIXTURES"
#include <cstdio>
#include <string>
#include <cllm/llm.hpp>         // 便利層：聚合 ask／expected 錯誤／串流糖／media helpers
#include <cllm/llm_reflect.hpp> // glaze 反射糖：ask_as<T>／make_tool<Args>／args_as<Args>

static llm::Client client_for(const std::string &base, const char *leaf) {
  llm::Client c;
  if (!base.empty()) c.endpoint = base + leaf;
  return c;
}

// ⚠ 給 glaze 反射的 struct 要放檔案層級（namespace scope）——函式內 local struct 反射不了。
struct Character {
  std::string name;
  int affection{};
  std::vector<std::string> lines;
};
struct GetWeather {
  std::string city, unit;
};

int main(int argc, char **argv) {
  std::string base = argc > 1 ? argv[1] : "";

  // 1) 基本 ask：一句話問、拿 Result<std::string>（失敗走 .error()，不混空字串）
  auto basic = client_for(base, "fake/chat/completions").ask("你好");
  std::printf("[cpp] ask => %s\n", basic ? basic->c_str() : basic.error().message.c_str());

  // 2) 串流：on_delta 逐段（回 true 可中止），回傳仍是聚合後的整段
  auto whole = client_for(base, "fake_stream/chat/completions")
                   .ask("數到五", {.stream = true, .on_delta = [](std::string_view sv) {
                          std::printf("[%.*s]", static_cast<int>(sv.size()), sv.data());
                          return false;
                        }});
  std::printf("\n[cpp] stream 聚合 => %s\n", whole ? whole->c_str() : whole.error().message.c_str());

  // 3) 結構化輸出：struct 即唯一真相源（反射生 schema → 回應解回 struct，零手刻 JSON）
  if (auto c = llm::ask_as<Character>(client_for(base, "fake_json/chat/completions"), "給我角色"))
    std::printf("[cpp] ask_as<T> => name=%s affection=%d lines=%zu\n",
                c->name.c_str(), c->affection, c->lines.size());
  else
    std::printf("[cpp] ask_as<T> 失敗 => %s\n", c.error().message.c_str());

  // 4) 工具呼叫：make_tool<Args> 反射參數 schema、args_as<Args> 解回型別（單輪，執行由你決定）
  auto calls = client_for(base, "fake_tool/chat/completions")
                   .ask_tools("東京天氣如何？", {llm::make_tool<GetWeather>("get_weather", "查某城市天氣")});
  if (calls)
    for (const auto &tc : *calls)
      if (auto args = llm::args_as<GetWeather>(tc))
        std::printf("[cpp] tool => %s(city=%s, unit=%s)\n",
                    tc.name.c_str(), args->city.c_str(), args->unit.c_str());

  // 5) shell 呼叫 llm CLI（unix filter）
  std::string cmd = "llm 你好 --endpoint '" + base + "fake/chat/completions'";
  std::string out;
  if (FILE *p = popen(cmd.c_str(), "r")) {
    char b[512]; size_t n;
    while ((n = fread(b, 1, sizeof b, p)) > 0) out.append(b, n);
    pclose(p);
  }
  std::printf("[cpp] shell(llm) => %s", out.c_str());
  return 0;
}
