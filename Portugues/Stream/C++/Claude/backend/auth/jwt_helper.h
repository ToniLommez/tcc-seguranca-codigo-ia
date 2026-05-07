#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class JwtHelper {
public:
    // Creates an HS256 JWT (our own tokens for API auth)
    static std::string createToken(const std::string& userId,
                                   const std::string& email,
                                   const std::string& userType,
                                   const std::string& secret);

    // Returns true and fills payload if token is valid and not expired
    static bool validateToken(const std::string& token,
                              const std::string& secret,
                              nlohmann::json& payload);

    // Creates an RS256 JWT for Google OAuth2 service account flow
    static std::string createServiceAccountJwt(const std::string& clientEmail,
                                               const std::string& privateKeyPem,
                                               const std::string& scope,
                                               const std::string& audience);

    static std::string base64UrlEncode(const unsigned char* data, size_t len);
    static std::string base64UrlEncodeStr(const std::string& s);
    static std::vector<unsigned char> base64UrlDecode(const std::string& input);
};
