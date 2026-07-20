// translate.cpp — OpenAI ⇄ Anthropic 純翻譯層實作（glaze json_t 動態重塑）。
//
// 港自舊 Python translate.py，一比一對應：system 抽頂層、content 字串/陣列、image（data URI
// /http url）、tools、response_format(json_schema)→強制工具、多輪工具往返、串流事件翻譯。

#include "translate.hpp"

namespace axl {
namespace axl_impl {  // 具名子 namespace（對齊專案慣例）

J jstr(std::string s) { J j; j = std::move(s); return j; }
J jnum(double d) { J j; j = d; return j; }
J jbool(bool b) { J j; j = b; return j; }
J jobj() { J j; j = J::object_t{}; return j; }
J jarr() { J j; j = J::array_t{}; return j; }

bool has(J &j, const char *k) { return j.is_object() && j.contains(k); }

std::string sget(J &j, const char *k, std::string d = "") {
  return has(j, k) && j[k].is_string() ? j[k].get_string() : d;
}

std::string dump(J &j) {
  std::string s;
  (void)glz::write_json(j, s);
  return s;
}

bool truthy(J &j) {
  if (j.is_boolean()) return j.get_boolean();
  if (j.is_number()) return j.get_number() != 0;
  return !j.is_null();
}

std::string join(const std::vector<std::string> &v, const std::string &sep) {
  std::string r;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) r += sep;
    r += v[i];
  }
  return r;
}

// data:<mime>;base64,<payload> → Anthropic base64 image source；不是就回 null。
J data_uri_source(const std::string &url) {
  if (url.rfind("data:", 0) != 0) return {};  // is_null()
  size_t comma = url.find(',');
  if (comma == std::string::npos) return {};
  std::string head = url.substr(5, comma - 5);           // "<mime>;base64"
  std::string payload = url.substr(comma + 1);
  std::string mime = head.substr(0, head.find(';'));
  if (mime.empty()) mime = "application/octet-stream";
  J src = jobj();
  src["type"] = jstr("base64");
  src["media_type"] = jstr(mime);
  src["data"] = jstr(payload);
  return src;
}

std::string content_to_text(J &content) {
  return content.is_string() ? content.get_string() : dump(content);
}

// OpenAI message.content（字串或 part 陣列）→ Anthropic content blocks。
J content_to_blocks(J &content) {
  J blocks = jarr();
  if (content.is_null()) return blocks;
  if (content.is_string()) {
    if (!content.get_string().empty()) {
      J b = jobj();
      b["type"] = jstr("text");
      b["text"] = content;
      blocks.get_array().push_back(std::move(b));
    }
    return blocks;
  }
  if (!content.is_array()) return blocks;
  for (J &part : content.get_array()) {
    std::string t = sget(part, "type");
    if (t == "text") {
      J b = jobj();
      b["type"] = jstr("text");
      b["text"] = jstr(sget(part, "text"));
      blocks.get_array().push_back(std::move(b));
    } else if (t == "image_url") {
      std::string url = has(part, "image_url") ? sget(part["image_url"], "url") : "";
      if (url.empty()) continue;
      J src = data_uri_source(url);
      if (src.is_null()) {
        src = jobj();
        src["type"] = jstr("url");
        src["url"] = jstr(url);
      }
      J b = jobj();
      b["type"] = jstr("image");
      b["source"] = std::move(src);
      blocks.get_array().push_back(std::move(b));
    }
    // input_audio：Anthropic Messages 不吃音訊輸入 → 靜默略過
  }
  return blocks;
}

J assistant_to_blocks(J &msg) {
  J blocks = has(msg, "content") ? content_to_blocks(msg["content"]) : jarr();
  if (has(msg, "tool_calls") && msg["tool_calls"].is_array()) {
    for (J &tc : msg["tool_calls"].get_array()) {
      if (!has(tc, "function")) continue;
      J &fn = tc["function"];
      J args = jobj();
      if (has(fn, "arguments") && fn["arguments"].is_string()) {
        J parsed;
        if (!glz::read_json(parsed, fn["arguments"].get_string()) && parsed.is_object())
          args = std::move(parsed);
      }
      J b = jobj();
      b["type"] = jstr("tool_use");
      b["id"] = jstr(sget(tc, "id"));
      b["name"] = jstr(sget(fn, "name"));
      b["input"] = std::move(args);
      blocks.get_array().push_back(std::move(b));
    }
  }
  return blocks;
}

J tool_result_to_blocks(J &msg) {
  J b = jobj();
  b["type"] = jstr("tool_result");
  b["tool_use_id"] = jstr(sget(msg, "tool_call_id"));
  b["content"] = jstr(has(msg, "content") ? content_to_text(msg["content"]) : "");
  J arr = jarr();
  arr.get_array().push_back(std::move(b));
  return arr;
}

J oai_tools_to_anthropic(J &tools) {
  J out = jarr();
  for (J &t : tools.get_array()) {
    if (!has(t, "function")) continue;
    J &fn = t["function"];
    J tool = jobj();
    tool["name"] = jstr(sget(fn, "name"));
    tool["description"] = jstr(sget(fn, "description"));
    if (has(fn, "parameters")) {
      tool["input_schema"] = fn["parameters"];
    } else {
      J sch = jobj();
      sch["type"] = jstr("object");
      tool["input_schema"] = std::move(sch);
    }
    out.get_array().push_back(std::move(tool));
  }
  return out;
}

std::string stop_reason(J &anth) {
  std::string r = sget(anth, "stop_reason");
  if (r == "max_tokens") return "length";
  if (r == "tool_use") return "tool_calls";
  return "stop";  // end_turn / stop_sequence / 其餘
}

J sse_obj(J obj) {
  std::string s = "data: ";
  s += dump(obj);
  s += "\n\n";
  return jstr(std::move(s));  // 借 J 當字串載體，caller 取 get_string
}

}  // namespace axl_impl
using namespace axl_impl;

// ───────────────────── 送出：OpenAI → Anthropic ─────────────────────
J to_anthropic(J &oai, int default_max_tokens) {
  std::vector<std::string> systems;
  J messages = jarr();
  if (has(oai, "messages") && oai["messages"].is_array()) {
    for (J &m : oai["messages"].get_array()) {
      std::string role = sget(m, "role");
      if (role == "system") {
        if (has(m, "content")) systems.push_back(content_to_text(m["content"]));
      } else if (role == "assistant") {
        J mo = jobj();
        mo["role"] = jstr("assistant");
        mo["content"] = assistant_to_blocks(m);
        messages.get_array().push_back(std::move(mo));
      } else if (role == "tool") {
        J mo = jobj();
        mo["role"] = jstr("user");
        mo["content"] = tool_result_to_blocks(m);
        messages.get_array().push_back(std::move(mo));
      } else {  // user（含未知 role）
        J mo = jobj();
        mo["role"] = jstr("user");
        mo["content"] = has(m, "content") ? content_to_blocks(m["content"]) : jarr();
        messages.get_array().push_back(std::move(mo));
      }
    }
  }

  J body = jobj();
  body["model"] = jstr(sget(oai, "model"));
  body["messages"] = std::move(messages);

  int mx = default_max_tokens;
  if (has(oai, "max_tokens") && oai["max_tokens"].is_number()) {
    int v = static_cast<int>(oai["max_tokens"].get_number());
    if (v > 0) mx = v;
  }
  body["max_tokens"] = jnum(mx);

  if (!systems.empty()) body["system"] = jstr(join(systems, "\n\n"));
  if (has(oai, "stream") && truthy(oai["stream"])) body["stream"] = jbool(true);
  if (has(oai, "temperature") && oai["temperature"].is_number())
    body["temperature"] = jnum(oai["temperature"].get_number());
  if (has(oai, "top_p") && oai["top_p"].is_number())
    body["top_p"] = jnum(oai["top_p"].get_number());

  J tools =
      has(oai, "tools") && oai["tools"].is_array() ? oai_tools_to_anthropic(oai["tools"]) : jarr();

  if (has(oai, "response_format") && oai["response_format"].is_object()) {
    J &rf = oai["response_format"];
    if (sget(rf, "type") == "json_schema" && has(rf, "json_schema")) {
      J &js = rf["json_schema"];
      J schema;
      if (has(js, "schema")) {
        schema = js["schema"];
      } else {
        schema = jobj();
        schema["type"] = jstr("object");
      }
      J tool = jobj();
      tool["name"] = jstr(kStructTool);
      tool["description"] = jstr("Return the result strictly as this JSON object.");
      tool["input_schema"] = std::move(schema);
      tools.get_array().push_back(std::move(tool));
      J tc = jobj();
      tc["type"] = jstr("tool");
      tc["name"] = jstr(kStructTool);
      body["tool_choice"] = std::move(tc);
    }
  }
  if (!tools.get_array().empty()) body["tools"] = std::move(tools);
  return body;
}

// ───────────────────── 收回：Anthropic → OpenAI ─────────────────────
J from_anthropic(J &anth) {
  std::vector<std::string> text_parts;
  J tool_calls = jarr();
  bool has_struct = false;
  std::string struct_json;

  if (has(anth, "content") && anth["content"].is_array()) {
    for (J &block : anth["content"].get_array()) {
      std::string bt = sget(block, "type");
      if (bt == "text") {
        text_parts.push_back(sget(block, "text"));
      } else if (bt == "tool_use") {
        J input = has(block, "input") ? block["input"] : jobj();
        if (sget(block, "name") == kStructTool) {
          has_struct = true;
          struct_json = dump(input);
        } else {
          J fn = jobj();
          fn["name"] = jstr(sget(block, "name"));
          fn["arguments"] = jstr(dump(input));
          J call = jobj();
          call["id"] = jstr(sget(block, "id"));
          call["type"] = jstr("function");
          call["function"] = std::move(fn);
          tool_calls.get_array().push_back(std::move(call));
        }
      }
    }
  }

  J message = jobj();
  message["role"] = jstr("assistant");
  message["content"] = jstr(has_struct ? struct_json : join(text_parts, ""));
  if (!tool_calls.get_array().empty()) message["tool_calls"] = std::move(tool_calls);

  J choice = jobj();
  choice["index"] = jnum(0);
  choice["message"] = std::move(message);
  choice["finish_reason"] = jstr(stop_reason(anth));
  J choices = jarr();
  choices.get_array().push_back(std::move(choice));

  J out = jobj();
  out["id"] = jstr(has(anth, "id") ? sget(anth, "id") : "chatcmpl-proxy");
  out["object"] = jstr("chat.completion");
  out["model"] = jstr(sget(anth, "model"));
  out["choices"] = std::move(choices);
  return out;
}

J err_to_openai(J &anth_err) {
  std::string msg;
  if (has(anth_err, "error") && anth_err["error"].is_object())
    msg = sget(anth_err["error"], "message");
  if (msg.empty()) msg = dump(anth_err);
  J e = jobj();
  e["message"] = jstr(msg);
  J out = jobj();
  out["error"] = std::move(e);
  return out;
}

// ───────────────────── 串流：Anthropic SSE → OpenAI SSE ─────────────────────
std::vector<std::string> StreamXlate::feed(J &data) {
  std::string t = sget(data, "type");
  int idx = has(data, "index") && data["index"].is_number()
                ? static_cast<int>(data["index"].get_number())
                : 0;

  auto one = [](J delta_choice) {
    J ch = jobj();
    ch["index"] = jnum(0);
    ch["delta"] = std::move(delta_choice);
    J arr = jarr();
    arr.get_array().push_back(std::move(ch));
    J o = jobj();
    o["choices"] = std::move(arr);
    return sse_obj(std::move(o)).get_string();
  };

  if (t == "content_block_start") {
    if (!has(data, "content_block")) return {};
    J &block = data["content_block"];
    if (sget(block, "type") == "tool_use") {
      if (sget(block, "name") == kStructTool) {
        struct_idx_.insert(idx);  // 之後這塊的 partial_json 當文字吐
        return {};
      }
      J fn = jobj();
      fn["name"] = jstr(sget(block, "name"));
      fn["arguments"] = jstr("");
      J tc = jobj();
      tc["index"] = jnum(idx);
      tc["id"] = jstr(sget(block, "id"));
      tc["type"] = jstr("function");
      tc["function"] = std::move(fn);
      J tcs = jarr();
      tcs.get_array().push_back(std::move(tc));
      J delta = jobj();
      delta["tool_calls"] = std::move(tcs);
      return {one(std::move(delta))};
    }
    return {};
  }

  if (t == "content_block_delta") {
    J &delta = data["delta"];
    std::string dt = sget(delta, "type");
    if (dt == "text_delta") {
      J d = jobj();
      d["content"] = jstr(sget(delta, "text"));
      return {one(std::move(d))};
    }
    if (dt == "input_json_delta") {
      std::string frag = sget(delta, "partial_json");
      if (struct_idx_.count(idx)) {
        J d = jobj();
        d["content"] = jstr(frag);
        return {one(std::move(d))};
      }
      J fn = jobj();
      fn["arguments"] = jstr(frag);
      J tc = jobj();
      tc["index"] = jnum(idx);
      tc["function"] = std::move(fn);
      J tcs = jarr();
      tcs.get_array().push_back(std::move(tc));
      J d = jobj();
      d["tool_calls"] = std::move(tcs);
      return {one(std::move(d))};
    }
    return {};
  }

  if (t == "message_stop") {
    J ch = jobj();
    ch["index"] = jnum(0);
    ch["delta"] = jobj();
    ch["finish_reason"] = jstr("stop");
    J arr = jarr();
    arr.get_array().push_back(std::move(ch));
    J o = jobj();
    o["choices"] = std::move(arr);
    return {sse_obj(std::move(o)).get_string(), "data: [DONE]\n\n"};
  }

  if (t == "error") return {sse_obj(err_to_openai(data)).get_string()};

  return {};  // message_start / ping / content_block_stop / message_delta
}

}  // namespace axl
