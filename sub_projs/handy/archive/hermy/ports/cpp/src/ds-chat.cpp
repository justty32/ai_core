// ds-chat — 一次 DeepSeek 呼叫：stdin={"messages":[...],"tools":[...]} → stdout=回應 message JSON。
// 「腦」的最小單元：HTTP 靠 curl、JSON 靠 jq，無狀態；messages 由編排器自管。
#include "proc.hpp"

int main() {
    std::string key = env_or("DEEPSEEK_API_KEY", "");
    if (key.empty()) { std::cerr << "[ds-chat] 未設 DEEPSEEK_API_KEY\n"; return 2; }
    std::string model = env_or("HERMY_MODEL", "deepseek-chat");
    std::string endpoint = env_or("HERMY_ENDPOINT", "https://api.deepseek.com/v1/chat/completions");

    std::string req = slurp_stdin(); if (req.empty()) req = "{}";
    // 用 jq 組請求體：有 tools 才加 tools + tool_choice=auto。
    const std::string filt =
        "{model:$model,messages:(.messages // [])} + "
        "(if ((.tools|length)>0) then {tools:.tools,tool_choice:\"auto\"} else {} end)";
    Proc b = run_proc({"jq", "-c", "--arg", "model", model, filt}, req);
    if (b.code != 0) { std::cerr << "[ds-chat] 請求組裝失敗：" << strip(b.err) << "\n"; return 1; }

    // curl POST；-w 在尾端多印 \n<http_code> 以便取狀態碼。
    Proc c = run_proc({"curl", "-sS", "-X", "POST", endpoint,
        "-H", "Content-Type: application/json",
        "-H", "Authorization: Bearer " + key,
        "--data-binary", "@-", "--max-time", "120", "-w", "\n%{http_code}"}, b.out);
    if (c.code != 0) { std::cerr << "[ds-chat] 請求失敗：curl exit " << c.code << " " << strip(c.err) << "\n"; return 1; }

    std::string resp = c.out;
    size_t nl = resp.rfind('\n');
    std::string payload = nl == std::string::npos ? "" : resp.substr(0, nl);
    int code = atoi((nl == std::string::npos ? resp : resp.substr(nl + 1)).c_str());
    if (code >= 400) {
        std::cerr << "[ds-chat] HTTP " << code << ": " << payload.substr(0, 500) << "\n"; return 1;
    }
    Proc m = run_proc({"jq", "-c", ".choices[0].message"}, payload);
    if (m.code != 0) { std::cerr << "[ds-chat] 回應解析失敗：" << strip(m.err) << "\n"; return 1; }
    std::cout << m.out;  // jq 已含換行
}
