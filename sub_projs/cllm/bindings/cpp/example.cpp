// example.cpp — cllm C++ 鏡像 llm::abi + jansson(JSON) + shell(CLI) 呼叫。
// 編：g++ -std=c++20 example.cpp $(pkg-config --cflags --libs cllm jansson) -o example
// 跑：source ~/repo/dev/env.sh 後  ./example "$CLLM_FIXTURES"
#include <cstdio>
#include <string>
#include <string_view>
#include <cllm/cabi.hpp>
#include <jansson.h>

static std::string ask(const std::string &base, const char *leaf, const char *prompt, const char *schema) {
  llm::abi::Client c; if (!base.empty()) c.endpoint = base + leaf;
  llm::abi::Request r{.prompt = prompt};
  if (schema) r.schema = llm::abi::Schema{.json = schema};
  std::string acc;
  c.ask(r, {.on_text = [&](std::string_view sv) { acc.append(sv); return false; }});
  return acc;
}
int main(int argc, char **argv) {
  std::string base = argc > 1 ? argv[1] : "";
  std::printf("[cpp] ask => %s\n", ask(base, "fake/chat/completions", "你好", nullptr).c_str());

  // schema → JSON → jansson
  std::string raw = ask(base, "fake_json/chat/completions", "給我角色", "{\"type\":\"object\"}");
  json_error_t err; json_t *root = json_loads(raw.c_str(), 0, &err);
  if (root) {
    std::printf("[cpp] json => name=%s affection=%lld lines=%zu\n",
                json_string_value(json_object_get(root, "name")),
                (long long)json_integer_value(json_object_get(root, "affection")),
                json_array_size(json_object_get(root, "lines")));
    json_decref(root);
  }
  // shell 呼叫
  std::string cmd = "llm 你好 --endpoint '" + base + "fake/chat/completions'";
  std::string out; if (FILE *p = popen(cmd.c_str(), "r")) { char b[512]; size_t n; while ((n = fread(b, 1, sizeof b, p)) > 0) out.append(b, n); pclose(p); }
  std::printf("[cpp] shell(llm) => %s", out.c_str());
  return 0;
}
