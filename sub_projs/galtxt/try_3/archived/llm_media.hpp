// llm_media.hpp — galtxt try_3：LLM 多媒體輸入（vision，圖片）。
//
// 在 llm::Client 之上長「帶文字＋圖片發問」。OpenAI 相容的多模態格式：user 訊息的 content
// 從「一個字串」變成「一個陣列」，每格是 {type:"text",text} 或 {type:"image_url",image_url:{url}}。
// 圖片可給 http(s):// URL（後端自己抓），或用 image_from_file 讀本地檔 → base64 data URI（自含）。

#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "llm.hpp"

namespace llm {

// 一張圖：url 可為 "https://…"（外部）或 "data:image/png;base64,…"（內嵌）。
struct Image {
    std::string url;
};

// 讀本地圖檔 → base64 data URI（glz::write_base64）。mime 例："image/png"、"image/jpeg"、"image/webp"。
// 檔案讀不到會 throw。產物自含（不依賴外部可達性），離線可用。
Image image_from_file(std::string_view path, std::string_view mime);

// 直接用外部 URL（後端負責去抓）。
Image image_from_url(std::string url);

// 帶文字＋若干圖片發問（vision）。回模型答覆文字。
// 非串流（先做這段；要串流可日後比照 Client::ask 補）。傳輸失敗會 throw。
std::string ask_vision(const Client& client,
                       std::string_view text,
                       const std::vector<Image>& images);

}  // namespace llm
