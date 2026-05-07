#include "jwt_utils.h"
#include "hash_utils.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <chrono>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

namespace JwtUtils {

// ---- Helpers -----------------------------------------------------------

static std::string makeJwtHS256(const json& payload, const std::string& secret) {
    json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    std::string h = HashUtils::base64UrlEncodeStr(header.dump());
    std::string p = HashUtils::base64UrlEncodeStr(payload.dump());
    std::string sigInput = h + "." + p;
    std::string sig = HashUtils::hmacSha256(sigInput, secret);
    return sigInput + "." + sig;
}

static std::string signRS256(const std::string& data,
                              const std::string& pemPrivateKey) {
    BIO* bio = BIO_new_mem_buf(pemPrivateKey.c_str(), -1);
    if (!bio) throw std::runtime_error("BIO_new_mem_buf failed");

    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) throw std::runtime_error("PEM_read_bio_PrivateKey failed - invalid key");

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("EVP_DigestSignInit failed");
    }

    EVP_DigestSignUpdate(ctx, data.c_str(), data.size());

    size_t sigLen = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sigLen);

    std::vector<unsigned char> sig(sigLen);
    EVP_DigestSignFinal(ctx, sig.data(), &sigLen);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return HashUtils::base64UrlEncode(sig.data(), sigLen);
}

// ---- Application JWT ---------------------------------------------------

std::string createToken(const std::string& userId,
                        const std::string& email,
                        const std::string& secret) {
    using namespace std::chrono;
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    json payload = {
        {"sub",   userId},
        {"email", email},
        {"iat",   now},
        {"exp",   now + 86400}  // 24 hours
    };
    return makeJwtHS256(payload, secret);
}

std::tuple<bool, std::string, std::string>
verifyToken(const std::string& token, const std::string& secret) {
    auto dot1 = token.find('.');
    auto dot2 = token.rfind('.');
    if (dot1 == std::string::npos || dot1 == dot2)
        return {false, "", ""};

    std::string header  = token.substr(0, dot1);
    std::string payload = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig     = token.substr(dot2 + 1);

    // Verify signature
    std::string expected = HashUtils::hmacSha256(header + "." + payload, secret);
    if (expected != sig) return {false, "", ""};

    // Decode payload
    auto payloadBytes = HashUtils::base64UrlDecode(payload);
    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());

    try {
        json claims = json::parse(payloadStr);
        using namespace std::chrono;
        auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

        if (claims.value("exp", 0LL) < now) return {false, "", ""};

        std::string userId = claims.value("sub",   "");
        std::string email  = claims.value("email", "");
        return {true, userId, email};
    } catch (...) {
        return {false, "", ""};
    }
}

// ---- Google Service Account JWT ----------------------------------------

std::string createGoogleJwt(const std::string& serviceAccountEmail,
                             const std::string& pemPrivateKey,
                             const std::string& privateKeyId) {
    using namespace std::chrono;
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    json header = {
        {"alg", "RS256"},
        {"typ", "JWT"},
        {"kid", privateKeyId}
    };
    json payload = {
        {"iss",   serviceAccountEmail},
        {"sub",   serviceAccountEmail},
        {"aud",   "https://oauth2.googleapis.com/token"},
        {"iat",   now},
        {"exp",   now + 3600},
        {"scope", "https://www.googleapis.com/auth/datastore "
                  "https://www.googleapis.com/auth/firebase"}
    };

    std::string h = HashUtils::base64UrlEncodeStr(header.dump());
    std::string p = HashUtils::base64UrlEncodeStr(payload.dump());
    std::string sigInput = h + "." + p;

    // The private key in the JSON uses literal \n — replace them
    std::string pem = pemPrivateKey;
    std::string needle = "\\n";
    size_t pos = 0;
    while ((pos = pem.find(needle, pos)) != std::string::npos) {
        pem.replace(pos, 2, "\n");
    }

    std::string sig = signRS256(sigInput, pem);
    return sigInput + "." + sig;
}

} // namespace JwtUtils
