// store.hpp — llm-login 的狀態層：路徑探測、JSON 檔讀寫（token 0600）、patch cllm config。介面。
//
// 只碰三個檔（都在 ~/.config/llm/，可用 opts/env 改）：
//   oauth.json（provider 設定，使用者填）／oauth_token.json（本工具寫，0600）／
//   config.json（cllm 的；只 patch api_key/endpoint/model，其餘鍵原樣保留）。

#pragma once
#include <string>

#include <glaze/glaze.hpp>

namespace login::store {

using J = glz::json_t;

std::string home_dir();
std::string default_path(const std::string &filename);  // ~/.config/llm/<filename>

// 讀整個檔→解析 JSON。讀不到/解不動 throw std::runtime_error（訊息含路徑）。
J read_json(const std::string &path);
// 寫字串到檔（mode0600＝POSIX 設 0600 權限，token 用）。失敗 throw。
void write_file(const std::string &path, const std::string &content, bool mode0600);

// 由 raw token 回應（access_token/expires_in/refresh_token…）算出存檔記錄：補 expires_at。
J make_token_record(J &raw, long now_unix);
// 憑證是否已過期（含 60s skew）；無 expires_at（不過期型）永遠回 false。
bool is_expired(J &rec, long now_unix);

// 只 patch cllm config.json 的 api_key（endpoint/model 非空才寫）；其餘鍵原樣保留。回實際路徑。
std::string patch_cllm(const std::string &config_path, const std::string &token,
                       const std::string &endpoint, const std::string &model);

}  // namespace login::store
