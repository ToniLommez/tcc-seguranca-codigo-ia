#include "password_service.hpp"

#include <vector>

#include <windows.h>
#include <bcrypt.h>

#include "utils.hpp"

namespace {

bool derive_pbkdf2(
    const std::string& password,
    const std::vector<unsigned char>& salt,
    std::vector<unsigned char>& output,
    std::string& error
) {
    BCRYPT_ALG_HANDLE algorithm{};
    const NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG
    );

    if (status < 0) {
        error = "BCryptOpenAlgorithmProvider failed.";
        return false;
    }

    output.resize(32);
    const NTSTATUS derive_status = BCryptDeriveKeyPBKDF2(
        algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(salt.data()),
        static_cast<ULONG>(salt.size()),
        120000,
        output.data(),
        static_cast<ULONG>(output.size()),
        0
    );

    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (derive_status < 0) {
        error = "BCryptDeriveKeyPBKDF2 failed.";
        return false;
    }

    return true;
}

}  // namespace

bool PasswordService::hash_password(
    const std::string& password,
    std::string& encoded_salt,
    std::string& encoded_hash,
    std::string& error
) {
    const auto salt = util::random_bytes(16);
    std::vector<unsigned char> derived;

    if (!derive_pbkdf2(password, salt, derived, error)) {
        return false;
    }

    encoded_salt = util::base64_url_encode(salt);
    encoded_hash = util::base64_url_encode(derived);
    return true;
}

bool PasswordService::verify_password(
    const std::string& password,
    const std::string& encoded_salt,
    const std::string& encoded_hash,
    std::string& error
) {
    const auto salt = util::base64_url_decode(encoded_salt);
    std::vector<unsigned char> derived;

    if (!derive_pbkdf2(password, salt, derived, error)) {
        return false;
    }

    const auto actual = util::base64_url_encode(derived);
    return util::constant_time_equals(actual, encoded_hash);
}
