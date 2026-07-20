// crypto.hpp — llm-login 自備的最小密碼學工具（PKCE 用）。介面。
//
// cllm 無 crypto 依賴，故 vendored：SHA-256（~公有領域實作）＋base64url＋安全亂數。
// 只夠 OAuth 2.0 PKCE 用（S256 challenge＝base64url(sha256(verifier))），不是通用密碼庫。

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace login::crypto {

// SHA-256：回 32 bytes 摘要。
std::vector<uint8_t> sha256(const std::string &data);

// base64url（RFC 4648 §5，無 padding）：+/→-_、去掉 =。
std::string base64url_nopad(const std::vector<uint8_t> &bytes);

// 安全亂數 n bytes（POSIX /dev/urandom；Windows BCryptGenRandom）。失敗 throw。
std::vector<uint8_t> random_bytes(size_t n);

// PKCE：回 {verifier, challenge}。verifier＝base64url(隨機 32B)；challenge＝base64url(sha256(verifier))。
std::pair<std::string, std::string> gen_pkce();

}  // namespace login::crypto
