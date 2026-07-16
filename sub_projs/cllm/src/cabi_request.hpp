#pragma once
// cabi_request.hpp — C++ 薄鏡像的「輸入」型（對應 C ABI 的 cabi_request.h）。
// 純 C++ struct，不依賴 C ABI；搬進 llm_ask 前才在 Client::ask（cabi.hpp）轉成 C 結構。

#include <optional>
#include <string>
#include <vector>

namespace llm::abi {

struct ToolDef {
  std::string name, description, parameters;
}; // parameters＝參數的 JSON Schema 字串（自備；反射生成留給上層便利層）
struct Schema {
  std::string name = "response";
  std::string json; // JSON Schema「物件」字串
};

// (a) 輸入媒體：url（https:// 或 data URI）或 bytes 二選一（都給時 url 優先）；種類看 mime。
struct MediaIn {
  std::string url;
  std::string mime;  // 走 data URI 時可省；走 bytes 時必帶
  std::string bytes; // 原始位元組（免先 base64；對應 C ABI 的 data/len）
};
// (c) 想要的輸出模態（請求指令，非媒體）：模態名 + 該模態生成參數的 JSON 物件字串。
struct Modality {
  std::string name;   // "text"/"audio"/"image"/"video"
  std::string config; // 該模態參數 JSON；空＝默認
};

// 一次發問的輸入（對應 llm_request_t）。schema／tools／media／modalities 可任意組合；
// stream 與 tools 正交——text/schema/media 皆可串流，tool_calls 一律拼完整才交給 on_tool。
struct Request {
  std::string prompt;
  std::optional<Schema> schema;
  std::vector<ToolDef> tools;
  std::vector<MediaIn> media;
  std::vector<Modality> modalities;
  bool stream = false;
};

} // namespace llm::abi
