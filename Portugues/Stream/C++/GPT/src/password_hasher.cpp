#include "password_hasher.h"

#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include "utils.h"

namespace {
constexpr int kIterations = 120000;
constexpr int kKeyLength = 32;
} // namespace

std::string PasswordHasher::hash_password(const std::string& password) {
  const auto salt = utils::random_bytes(16);
  std::vector<unsigned char> derived(kKeyLength);

  if (PKCS5_PBKDF2_HMAC(password.c_str(),
                        static_cast<int>(password.size()),
                        salt.data(),
                        static_cast<int>(salt.size()),
                        kIterations,
                        EVP_sha256(),
                        static_cast<int>(derived.size()),
                        derived.data()) != 1) {
    throw std::runtime_error("Falha ao derivar hash da senha.");
  }

  return "pbkdf2_sha256$" + std::to_string(kIterations) + "$" + utils::hex_encode(salt) + "$" +
         utils::hex_encode(derived);
}

bool PasswordHasher::verify_password(const std::string& password, const std::string& stored_hash) {
  std::stringstream ss(stored_hash);
  std::string algorithm;
  std::string iterations_text;
  std::string salt_hex;
  std::string expected_hex;

  if (!std::getline(ss, algorithm, '$') || !std::getline(ss, iterations_text, '$') ||
      !std::getline(ss, salt_hex, '$') || !std::getline(ss, expected_hex, '$')) {
    return false;
  }

  if (algorithm != "pbkdf2_sha256") {
    return false;
  }

  const int iterations = std::stoi(iterations_text);
  const auto salt = utils::hex_decode(salt_hex);
  const auto expected = utils::hex_decode(expected_hex);

  std::vector<unsigned char> derived(expected.size());
  if (PKCS5_PBKDF2_HMAC(password.c_str(),
                        static_cast<int>(password.size()),
                        salt.data(),
                        static_cast<int>(salt.size()),
                        iterations,
                        EVP_sha256(),
                        static_cast<int>(derived.size()),
                        derived.data()) != 1) {
    return false;
  }

  return derived.size() == expected.size() &&
         CRYPTO_memcmp(derived.data(), expected.data(), expected.size()) == 0;
}
