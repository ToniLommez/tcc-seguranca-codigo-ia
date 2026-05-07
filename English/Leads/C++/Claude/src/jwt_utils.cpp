#include "jwt_utils.h"

#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace jwt_utils {

static std::string g_secret;

void init(const std::string& secret) {
    g_secret = secret;
}

static std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return oss.str();
}

static std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned char byte = (unsigned char)std::stoi(hex.substr(i, 2), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string generate_salt() {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    return bytes_to_hex(salt, sizeof(salt));
}

std::string hash_password(const std::string& password, const std::string& salt) {
    auto salt_bytes = hex_to_bytes(salt);
    unsigned char hash[32];
    PKCS5_PBKDF2_HMAC(
        password.c_str(), (int)password.length(),
        salt_bytes.data(), (int)salt_bytes.size(),
        100000, EVP_sha256(), sizeof(hash), hash
    );
    return bytes_to_hex(hash, sizeof(hash));
}

std::string create_user_token(const std::string& user_id, const std::string& email) {
    auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_issuer("lead-manager")
        .set_subject(user_id)
        .set_payload_claim("email", jwt::claim(email))
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours{24})
        .sign(jwt::algorithm::hs256{g_secret});
}

std::optional<std::pair<std::string, std::string>> verify_user_token(const std::string& token) {
    try {
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{g_secret})
            .with_issuer("lead-manager");
        auto decoded = jwt::decode(token);
        verifier.verify(decoded);
        auto user_id = decoded.get_subject();
        auto email = decoded.get_payload_claim("email").as_string();
        return std::make_pair(user_id, email);
    } catch (...) {
        return std::nullopt;
    }
}

std::string create_google_jwt(const std::string& client_email, const std::string& private_key) {
    auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_issuer(client_email)
        .set_subject(client_email)
        .set_audience("https://oauth2.googleapis.com/token")
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours{1})
        .set_payload_claim("scope", jwt::claim(std::string("https://www.googleapis.com/auth/datastore")))
        .sign(jwt::algorithm::rs256("", private_key, "", ""));
}

}
