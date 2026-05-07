#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace HashUtils {

// ---- Base64 / Base64URL ------------------------------------------------

inline std::string base64Encode(const unsigned char* data, size_t len) {
    static const char* b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int v = (unsigned int)data[i] << 16;
        if (i + 1 < len) v |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) v |= (unsigned int)data[i + 2];
        out += b64[(v >> 18) & 0x3F];
        out += b64[(v >> 12) & 0x3F];
        out += (i + 1 < len) ? b64[(v >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? b64[v & 0x3F]         : '=';
    }
    return out;
}

inline std::string base64UrlEncode(const unsigned char* data, size_t len) {
    std::string s = base64Encode(data, len);
    for (char& c : s) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    // Remove padding
    while (!s.empty() && s.back() == '=') s.pop_back();
    return s;
}

inline std::string base64UrlEncodeStr(const std::string& str) {
    return base64UrlEncode(
        reinterpret_cast<const unsigned char*>(str.data()), str.size());
}

inline std::vector<unsigned char> base64UrlDecode(const std::string& in) {
    std::string s = in;
    for (char& c : s) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (s.size() % 4 != 0) s += '=';

    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : s) {
        if (c == '=') break;
        int idx;
        if (c >= 'A' && c <= 'Z') idx = c - 'A';
        else if (c >= 'a' && c <= 'z') idx = c - 'a' + 26;
        else if (c >= '0' && c <= '9') idx = c - '0' + 52;
        else if (c == '+') idx = 62;
        else if (c == '/') idx = 63;
        else continue;
        val = (val << 6) | idx;
        bits += 6;
        if (bits >= 0) {
            out.push_back((unsigned char)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// ---- HMAC-SHA256 -------------------------------------------------------

inline std::string hmacSha256(const std::string& data, const std::string& key) {
    unsigned char digest[32];
    unsigned int  digestLen = 32;
    HMAC(EVP_sha256(),
         key.data(),  (int)key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), (int)data.size(),
         digest, &digestLen);
    return base64UrlEncode(digest, digestLen);
}

// ---- Password hashing (PBKDF2-HMAC-SHA256) -----------------------------

inline std::string generateSalt(size_t len = 16) {
    std::vector<unsigned char> buf(len);
    RAND_bytes(buf.data(), (int)len);
    std::ostringstream oss;
    for (auto b : buf) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

inline std::string hashPassword(const std::string& password, const std::string& salt) {
    unsigned char key[32];
    PKCS5_PBKDF2_HMAC(
        password.c_str(), (int)password.size(),
        reinterpret_cast<const unsigned char*>(salt.c_str()), (int)salt.size(),
        10000, EVP_sha256(), 32, key);
    return salt + ":" + base64UrlEncode(key, 32);
}

// Returns "<salt>:<hash>" that can be stored directly
inline std::string hashPasswordNew(const std::string& password) {
    std::string salt = generateSalt();
    return hashPassword(password, salt);
}

// Verifies password against stored "<salt>:<hash>"
inline bool verifyPassword(const std::string& password, const std::string& stored) {
    auto sep = stored.find(':');
    if (sep == std::string::npos) return false;
    std::string salt = stored.substr(0, sep);
    std::string expected = hashPassword(password, salt);
    return expected == stored;
}

// ---- UUID v4 -----------------------------------------------------------

inline std::string generateUUID() {
    unsigned char buf[16];
    RAND_bytes(buf, 16);
    // Set version 4
    buf[6] = (buf[6] & 0x0F) | 0x40;
    // Set variant bits
    buf[8] = (buf[8] & 0x3F) | 0x80;

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) oss << '-';
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
    }
    return oss.str();
}

} // namespace HashUtils
