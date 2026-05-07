#include "jwt_service.h"

#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/crypto.h>

#include <nlohmann/json.hpp>

#include "utils.h"

JwtService::JwtService(std::string secret, int expiration_hours)
  : secret_(std::move(secret)), expiration_hours_(expiration_hours) {}

std::string JwtService::issue_token(const AuthUser& user) const {
  nlohmann::json header = {
    {"alg", "HS256"},
    {"typ", "JWT"}
  };

  const auto now = utils::unix_now();
  nlohmann::json payload = {
    {"sub", user.email},
    {"name", user.name},
    {"email", user.email},
    {"type", user_type_to_string(user.type)},
    {"iat", now},
    {"exp", now + static_cast<std::int64_t>(expiration_hours_) * 3600}
  };

  const std::string header_encoded = utils::base64_url_encode(header.dump());
  const std::string payload_encoded = utils::base64_url_encode(payload.dump());
  const std::string signing_input = header_encoded + "." + payload_encoded;
  const auto signature = utils::hmac_sha256(secret_, signing_input);

  return signing_input + "." + utils::base64_url_encode(signature);
}

std::optional<AuthUser> JwtService::verify_token(const std::string& token) const {
  std::vector<std::string> parts;
  std::stringstream ss(token);
  std::string item;
  while (std::getline(ss, item, '.')) {
    parts.push_back(item);
  }

  if (parts.size() != 3) {
    return std::nullopt;
  }

  const std::string signing_input = parts[0] + "." + parts[1];
  const auto expected_signature = utils::hmac_sha256(secret_, signing_input);
  const auto provided_signature = utils::base64_url_decode(parts[2]);

  if (expected_signature.size() != provided_signature.size() ||
      CRYPTO_memcmp(expected_signature.data(), provided_signature.data(), expected_signature.size()) != 0) {
    return std::nullopt;
  }

  const auto header_bytes = utils::base64_url_decode(parts[0]);
  const auto payload_bytes = utils::base64_url_decode(parts[1]);

  const auto header = nlohmann::json::parse(std::string(header_bytes.begin(), header_bytes.end()));
  if (header.value("alg", "") != "HS256") {
    return std::nullopt;
  }

  const auto payload = nlohmann::json::parse(std::string(payload_bytes.begin(), payload_bytes.end()));
  if (payload.value("exp", 0LL) < utils::unix_now()) {
    return std::nullopt;
  }

  const auto type = user_type_from_string(payload.value("type", ""));
  if (!type.has_value()) {
    return std::nullopt;
  }

  return AuthUser{
    payload.value("name", ""),
    payload.value("email", ""),
    *type
  };
}
