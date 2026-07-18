// cabi_request.cpp — 組 OpenAI 請求 JSON（唯一真相源的「送出」半邊）。
//
// prompt(+media parts)／response_format(schema)／tools[]／modalities／取樣／stream 併成一包。
// 這是曾散在 L0 四個獨立入口（ask／ask_json／ask_tools／ask_vision）的請求組裝，收斂成一處。

#include "cabi_internal.hpp"

#include <optional>
#include <string>
#include <vector>

#include <glaze/glaze.hpp>

namespace cabi_impl {
// 具名子 namespace（非匿名）：避免 glaze quoted_key_v 的 COMDAT section 跨 TU 撞名
// （GCC 匿名 namespace 一律 mangle 成 _GLOBAL__N_1，同名 struct 折疊後給錯 JSON 鍵）。
namespace req_impl {

// glz::write_json 回 expected；統一收斂成字串（失敗給 fallback）。
template <class T>
std::string to_json(const T &v, const char *fallback = "{}") {
  auto r = glz::write_json(v);
  return r ? std::move(*r) : std::string(fallback);
}

// content 各格（文字／圖／音各自序列化，故不共存同一 struct）
struct TextPart {
  std::string type = "text";
  std::string text;
};
struct ImageUrl {
  std::string url;
};
struct ImagePart {
  std::string type = "image_url";
  ImageUrl image_url;
};
struct InputAudio {
  std::string data; // base64
  std::string format;
};
struct AudioPart {
  std::string type = "input_audio";
  InputAudio input_audio;
};

// message.content 用 raw_json：無 media＝字串、有 media＝異質陣列，兩形共存靠這個。
struct ReqMessage {
  std::string role = "user";
  glz::raw_json content;
};
// 工具定義：{type:"function", function:{name,description,parameters}}（parameters 是物件，raw 嵌入）
struct ReqFn {
  std::string name;
  std::string description;
  glz::raw_json parameters;
};
struct ReqTool {
  std::string type = "function";
  ReqFn function;
};
// 結構化輸出：{type:"json_schema", json_schema:{name,strict,schema}}
struct JsonSchema {
  std::string name;
  bool strict = true;
  glz::raw_json schema;
};
struct ResponseFormat {
  std::string type = "json_schema";
  JsonSchema json_schema;
};
struct ReqBody {
  std::optional<std::string> model;
  std::vector<ReqMessage> messages;
  bool stream = false;
  std::optional<float> temperature; // ↓ 取樣六欄（由 field_mask 決定送不送）
  std::optional<float> top_p;
  std::optional<int> max_tokens;
  std::optional<float> presence_penalty;
  std::optional<float> frequency_penalty;
  std::optional<int> seed;
  std::optional<ResponseFormat> response_format;
  std::optional<std::vector<ReqTool>> tools;
  std::optional<std::vector<std::string>> modalities;
};

// content：無 media → JSON 字串；有 media → [文字格, 媒體格…]。
glz::raw_json build_content(const llm_request_t *req) {
  std::string prompt = req->prompt ? req->prompt : "";
  if (!req->media || req->media_count == 0) {
    return glz::raw_json{to_json(prompt, "\"\"")}; // 字串會被正確加引號/轉義
  }
  std::vector<glz::raw_json> parts;
  parts.push_back(glz::raw_json{to_json(TextPart{.text = prompt})});
  for (size_t i = 0; i < req->media_count; ++i) {
    const llm_media_in_t &m = req->media[i];
    std::string mime = m.mime ? m.mime : "";
    bool is_audio = mime.rfind("audio/", 0) == 0;
    if (is_audio && m.data) { // 音訊輸入：input_audio{data(base64), format}
      AudioPart ap;
      ap.input_audio.data = glz::write_base64(std::string(m.data, m.len));
      ap.input_audio.format = mime.substr(6); // "audio/" 之後
      parts.push_back(glz::raw_json{to_json(ap)});
    } else { // 其餘（圖）：image_url，url 直用或 data → data URI
      ImagePart ip;
      if (m.url && *m.url) {
        ip.image_url.url = m.url;
      } else if (m.data) {
        std::string mt = mime.empty() ? "application/octet-stream" : mime;
        ip.image_url.url =
            "data:" + mt + ";base64," + glz::write_base64(std::string(m.data, m.len));
      }
      parts.push_back(glz::raw_json{to_json(ip)});
    }
  }
  return glz::raw_json{to_json(parts, "[]")};
}

} // namespace req_impl
using namespace req_impl;

// 組完整請求 JSON（含 modalities 的 per-modality config：glaze struct 無法動態鍵，故末尾拼接）。
std::string build_body(const llm_client_t *c, const llm_request_t *req) {
  ReqBody b;
  b.stream = req->stream != 0;

  // 取樣搬運（原 apply_sampling 那七行）：model 看指標非空、六個數值欄看 field_mask。
  if (c) {
    if (c->model)
      b.model = c->model;
    if (c->field_mask & LLM_FIELD_TEMPERATURE)
      b.temperature = c->temperature;
    if (c->field_mask & LLM_FIELD_TOP_P)
      b.top_p = c->top_p;
    if (c->field_mask & LLM_FIELD_MAX_TOKENS)
      b.max_tokens = c->max_tokens;
    if (c->field_mask & LLM_FIELD_PRESENCE_PENALTY)
      b.presence_penalty = c->presence_penalty;
    if (c->field_mask & LLM_FIELD_FREQUENCY_PENALTY)
      b.frequency_penalty = c->frequency_penalty;
    if (c->field_mask & LLM_FIELD_SEED)
      b.seed = c->seed;
  }

  // system role（若給）排在最前：OpenAI 慣例 system→user。內容永遠是純文字（不夾媒體）。
  if (req->system && *req->system) {
    ReqMessage sys;
    sys.role = "system";
    sys.content = glz::raw_json{to_json(std::string(req->system), "\"\"")};
    b.messages.push_back(std::move(sys));
  }

  ReqMessage msg;
  msg.content = build_content(req);
  b.messages.push_back(std::move(msg));

  if (req->schema) {
    ResponseFormat rf;
    rf.json_schema.name =
        (req->schema->name && *req->schema->name) ? req->schema->name : "response";
    rf.json_schema.schema =
        glz::raw_json{req->schema->json ? std::string(req->schema->json) : std::string("{}")};
    b.response_format = std::move(rf);
  }
  if (req->tools && req->tools_count > 0) {
    std::vector<ReqTool> tv;
    for (size_t i = 0; i < req->tools_count; ++i) {
      const llm_tool_def_t &t = req->tools[i];
      ReqTool rt;
      rt.function.name = t.name ? t.name : "";
      rt.function.description = t.description ? t.description : "";
      rt.function.parameters =
          glz::raw_json{t.parameters ? std::string(t.parameters) : std::string("{}")};
      tv.push_back(std::move(rt));
    }
    b.tools = std::move(tv);
  }

  // modalities：names 陣列進 struct；有 config 的另拼成頂層鍵（如 "audio":{…}）。
  std::string extra;
  if (req->modalities && req->modalities_count > 0) {
    std::vector<std::string> names;
    for (size_t i = 0; i < req->modalities_count; ++i) {
      const llm_modality_t &mo = req->modalities[i];
      std::string name = mo.name ? mo.name : "";
      if (!name.empty())
        names.push_back(name);
      if (mo.config && *mo.config && !name.empty())
        extra += ",\"" + name + "\":" + mo.config; // config 是 JSON 物件字串，原樣嵌
    }
    if (!names.empty())
      b.modalities = std::move(names);
  }

  std::string json = to_json(b);
  if (!extra.empty() && !json.empty() && json.back() == '}')
    json.insert(json.size() - 1, extra); // 插在最後的 '}' 前
  return json;
}

} // namespace cabi_impl
