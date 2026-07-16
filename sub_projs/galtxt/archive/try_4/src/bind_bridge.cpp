// bind_bridge.cpp — bind.hpp 的 bridge_ask 實作：唯一碰 llm::Client 的地方。
//
// 「一個引擎、兩層薄綁定」的引擎那一半。s7_bind.cpp／lua_bind.cpp 只把腳本傳來的東西轉成 AskOpts、
// 呼叫本函式、再把結果塞回各自 VM——真正的 LLM 呼叫全在這裡（借編 try_3 的 llm::Client）。
//
// ★ noexcept 是刻意的：呼叫端（VM 綁定）接著可能呼 lua_error／s7_error，那是 longjmp。
//   若本函式讓 std::exception 漏出去、又在 VM 的 C 幀裡被 longjmp 跨越，即未定義行為。
//   故在這條邊界把所有例外收斂成「bool＋err 字串」，讓上層用 VM 原生的錯誤路徑回報。
//
// 取樣選項的搬運哲學：只有「腳本明確設了」（optional 有值）才覆寫，否則保留 from_env 的預設——
// 對齊 try_3 的「struct＝唯一真相源、沒設就別替後端做主」。

#include "bind.hpp"

#include <exception>
#include <vector>

#include "llm.hpp"        // 借編 try_3/src 的 llm::Client（from_env／ask／OnDelta）
#include "llm_tool.hpp"   // ask_tools／Tool／ToolCall
#include "llm_json.hpp"   // ask_json_raw（結構化輸出的非模板入口）
#include "llm_media.hpp"  // ask_vision／image_from_url／image_from_file

namespace vmbind {

namespace {
// 小工具：依 endpoint 覆寫 from_env 的 Client（擴充也共用「空＝走預設」的規則）。
llm::Client make_client(const std::string& endpoint) {
    llm::Client c = llm::from_env();
    if (!endpoint.empty()) c.endpoint = endpoint;
    return c;
}
}  // namespace

bool bridge_ask(const AskOpts& o, std::string& out, std::string& err) noexcept {
    try {
        llm::Client c = llm::from_env();              // base：env（預設本機 LM Studio）
        if (!o.endpoint.empty()) c.endpoint = o.endpoint;  // 腳本給了才覆寫（離線 file:// fixture 靠這個）
        if (o.model)       c.model       = o.model;   // 以下取樣選項：optional 有值才覆寫
        if (o.temperature) c.temperature = o.temperature;
        if (o.top_p)       c.top_p       = o.top_p;
        if (o.max_tokens)  c.max_tokens  = o.max_tokens;
        if (o.seed)        c.seed        = o.seed;

        // 串流：把（可能為空的）on_delta 交給 ask；ask 內部對空 std::function 會略過（見 llm.cpp）。
        // 非串流：一次收完。兩種都回「完整組合後的答案」。
        out = o.stream ? c.ask(o.prompt, true, o.on_delta)
                       : c.ask(o.prompt);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    } catch (...) {
        err = "未知錯誤（非 std::exception）";
        return false;
    }
}

bool bridge_ask_json(const std::string& prompt, const std::string& endpoint,
                     const std::string& schema_name, const std::string& schema_json,
                     std::string& out_json, std::string& err) noexcept {
    try {
        llm::Client c = make_client(endpoint);
        // 非模板入口：schema 由腳本以字串給（不經 schema_of<T>），回被約束的 JSON 字串。
        out_json = llm::ask_json_raw(c, prompt, schema_name, schema_json);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    } catch (...) {
        err = "未知錯誤（非 std::exception）";
        return false;
    }
}

bool bridge_ask_tools(const std::string& prompt, const std::string& endpoint,
                      const std::vector<ToolSpec>& tools,
                      std::vector<ToolCallOut>& calls, std::string& err) noexcept {
    try {
        llm::Client c = make_client(endpoint);
        std::vector<llm::Tool> llm_tools;   // ToolSpec（腳本側）→ llm::Tool（核心側）
        llm_tools.reserve(tools.size());
        for (const auto& t : tools)
            llm_tools.push_back(llm::Tool{ t.name, t.description, t.schema });
        for (const auto& tc : llm::ask_tools(c, prompt, llm_tools))
            calls.push_back(ToolCallOut{ tc.name, tc.arguments });   // arguments 仍是 JSON 字串
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    } catch (...) {
        err = "未知錯誤（非 std::exception）";
        return false;
    }
}

bool bridge_ask_vision(const std::string& prompt, const std::string& endpoint,
                       const std::vector<ImageSpec>& images,
                       std::string& out, std::string& err) noexcept {
    try {
        llm::Client c = make_client(endpoint);
        std::vector<llm::Image> imgs;   // ImageSpec（腳本側）→ llm::Image（核心側）
        imgs.reserve(images.size());
        for (const auto& im : images) {
            if (!im.url.empty())
                imgs.push_back(llm::image_from_url(im.url));
            else if (!im.file.empty())
                imgs.push_back(llm::image_from_file(im.file, im.mime.empty() ? "image/png" : im.mime));
        }
        out = llm::ask_vision(c, prompt, imgs);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    } catch (...) {
        err = "未知錯誤（非 std::exception）";
        return false;
    }
}

}  // namespace vmbind
