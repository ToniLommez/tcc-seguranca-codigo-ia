#pragma once

#include <string>
#include <chrono>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include "models.h"

namespace soundstream {

class JwtAuth {
public:
    explicit JwtAuth(const std::string& secret) : secret_(secret) {}

    std::string create_token(const TokenPayload& payload) {
        auto now = std::chrono::system_clock::now();
        auto token = jwt::create()
            .set_issuer("soundstream")
            .set_type("JWT")
            .set_issued_at(now)
            .set_expires_at(now + std::chrono::hours(24))
            .set_payload_claim("user_id", jwt::claim(payload.user_id))
            .set_payload_claim("email", jwt::claim(payload.email))
            .set_payload_claim("type", jwt::claim(payload.type))
            .set_payload_claim("name", jwt::claim(payload.name))
            .sign(jwt::algorithm::hs256{secret_});
        return token;
    }

    TokenPayload verify_token(const std::string& token) {
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_})
            .with_issuer("soundstream");

        auto decoded = jwt::decode(token);
        verifier.verify(decoded);

        TokenPayload payload;
        payload.user_id = decoded.get_payload_claim("user_id").as_string();
        payload.email = decoded.get_payload_claim("email").as_string();
        payload.type = decoded.get_payload_claim("type").as_string();
        payload.name = decoded.get_payload_claim("name").as_string();
        return payload;
    }

private:
    std::string secret_;
};

} // namespace soundstream
