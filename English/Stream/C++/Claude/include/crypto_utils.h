#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <openssl/sha.h>
#include <openssl/rand.h>

namespace soundstream {

inline std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return oss.str();
}

inline std::string sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);
    return bytes_to_hex(hash, SHA256_DIGEST_LENGTH);
}

inline std::string generate_salt(size_t length = 32) {
    std::vector<unsigned char> buf(length);
    RAND_bytes(buf.data(), static_cast<int>(length));
    return bytes_to_hex(buf.data(), length);
}

inline std::string hash_password(const std::string& password, const std::string& salt) {
    return sha256(salt + password + salt);
}

inline bool verify_password(const std::string& password, const std::string& salt, const std::string& hash) {
    return hash_password(password, salt) == hash;
}

inline std::string generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::ostringstream ss;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) ss << '-';
        if (i == 12) ss << '4';
        else if (i == 16) ss << std::hex << dis2(gen);
        else ss << std::hex << dis(gen);
    }
    return ss.str();
}

} // namespace soundstream
