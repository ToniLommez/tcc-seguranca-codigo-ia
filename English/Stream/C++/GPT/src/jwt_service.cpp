#include "jwt_service.hpp"

#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils.hpp"

JwtService::JwtService(std::string secret)
    : secret_(std::move(secret)) {}

std::string JwtService::create_token(const UserRecord& user, int expires_in_minutes) const {
    const auto issued_at = util::now_unix_timestamp();
    const auto expires_at = issued_at + static_cast<std::int64_t>(expires_in_minutes) * 60;

    const nlohmann::json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };

    const nlohmann::json payload = {
        {"sub", user.id},
        {"name", user.name},
        {"email", user.email},
        {"role", role_to_string(user.role)},
        {"iat", issued_at},
        {"exp", expires_at}
    };

    const auto encoded_header = util::base64_url_encode(header.dump());
    const auto encoded_payload = util::base64_url_encode(payload.dump());
    const auto signing_input = encoded_header + "." + encoded_payload;
    const auto signature = util::hmac_sha256(secret_, signing_input);

    return signing_input + "." + util::base64_url_encode(signature);
}

std::optional<JwtClaims> JwtService::verify_token(const std::string& token) const {
    const auto first_dot = token.find('.');
    if (first_dot == std::string::npos) {
        return std::nullopt;
    }

    const auto second_dot = token.find('.', first_dot + 1);
    if (second_dot == std::string::npos) {
        return std::nullopt;
    }

    const auto encoded_header = token.substr(0, first_dot);
    const auto encoded_payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
    const auto encoded_signature = token.substr(second_dot + 1);

    const auto expected_signature = util::base64_url_encode(
        util::hmac_sha256(secret_, encoded_header + "." + encoded_payload)
    );

    if (!util::constant_time_equals(expected_signature, encoded_signature)) {
        return std::nullopt;
    }

    const auto payload_text = util::base64_url_decode_to_string(encoded_payload);
    const auto payload = nlohmann::json::parse(payload_text, nullptr, false);
    if (payload.is_discarded()) {
        return std::nullopt;
    }

    const auto role = role_from_string(payload.value("role", ""));
    if (!role.has_value()) {
        return std::nullopt;
    }

    JwtClaims claims;
    claims.subject = payload.value("sub", "");
    claims.name = payload.value("name", "");
    claims.email = payload.value("email", "");
    claims.role = *role;
    claims.issued_at = payload.value("iat", 0LL);
    claims.expires_at = payload.value("exp", 0LL);

    if (claims.subject.empty() || claims.email.empty() || util::now_unix_timestamp() >= claims.expires_at) {
        return std::nullopt;
    }

    return claims;
}

