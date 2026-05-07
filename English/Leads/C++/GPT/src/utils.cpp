#include "lead_manager/utils.hpp"

#include <windows.h>
#include <wincrypt.h>

#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/sha256.h>

namespace lead_manager::util {

namespace {

std::string replaceAll(std::string value, const std::string& from, const std::string& to) {
  std::size_t position = 0;
  while ((position = value.find(from, position)) != std::string::npos) {
    value.replace(position, from.size(), to);
    position += to.size();
  }
  return value;
}

std::string base64EncodeRaw(const unsigned char* data, std::size_t length) {
  std::vector<unsigned char> buffer(((length + 2) / 3) * 4 + 4);
  std::size_t outputLength = 0;
  if (mbedtls_base64_encode(buffer.data(), buffer.size(), &outputLength, data, length) != 0) {
    throw std::runtime_error("Failed to base64 encode content.");
  }
  return std::string(reinterpret_cast<char*>(buffer.data()), outputLength);
}

std::vector<unsigned char> base64DecodeRaw(const std::string& input) {
  std::vector<unsigned char> output(input.size() + 4);
  std::size_t outputLength = 0;
  if (mbedtls_base64_decode(output.data(), output.size(), &outputLength,
                            reinterpret_cast<const unsigned char*>(input.data()),
                            input.size()) != 0) {
    throw std::runtime_error("Failed to base64 decode content.");
  }
  output.resize(outputLength);
  return output;
}

std::string bytesToHex(const unsigned char* data, std::size_t length) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < length; ++index) {
    stream << std::setw(2) << static_cast<int>(data[index]);
  }
  return stream.str();
}

}  // namespace

std::string nowUtcIso() {
  const auto now = std::chrono::system_clock::now();
  const auto timeValue = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_s(&tm, &timeValue);
  std::ostringstream stream;
  stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::string currentDateIso() {
  const auto now = std::chrono::system_clock::now();
  const auto timeValue = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_s(&tm, &timeValue);
  std::ostringstream stream;
  stream << std::put_time(&tm, "%Y-%m-%d");
  return stream.str();
}

std::string trim(const std::string& input) {
  std::size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }
  std::size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }
  return input.substr(start, end - start);
}

std::string toLower(const std::string& input) {
  std::string result = input;
  for (char& character : result) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return result;
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string randomHex(std::size_t bytes) {
  std::vector<unsigned char> buffer(bytes);
  HCRYPTPROV provider = 0;
  if (!CryptAcquireContext(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    throw std::runtime_error("Failed to acquire crypto provider.");
  }
  const BOOL generated = CryptGenRandom(provider, static_cast<DWORD>(buffer.size()), buffer.data());
  CryptReleaseContext(provider, 0);
  if (!generated) {
    throw std::runtime_error("Failed to generate random bytes.");
  }
  return bytesToHex(buffer.data(), buffer.size());
}

std::string generateId() {
  return randomHex(16);
}

std::string base64UrlEncode(const std::string& data) {
  std::string encoded = base64EncodeRaw(reinterpret_cast<const unsigned char*>(data.data()), data.size());
  encoded = replaceAll(encoded, "+", "-");
  encoded = replaceAll(encoded, "/", "_");
  while (!encoded.empty() && encoded.back() == '=') {
    encoded.pop_back();
  }
  return encoded;
}

std::string base64UrlDecode(const std::string& data) {
  std::string normalized = replaceAll(replaceAll(data, "-", "+"), "_", "/");
  while ((normalized.size() % 4) != 0) {
    normalized.push_back('=');
  }
  const auto decoded = base64DecodeRaw(normalized);
  return std::string(reinterpret_cast<const char*>(decoded.data()), decoded.size());
}

std::string hmacSha256Base64Url(const std::string& key, const std::string& data) {
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr) {
    throw std::runtime_error("Unable to initialize SHA-256.");
  }

  unsigned char output[32];
  if (mbedtls_md_hmac(info, reinterpret_cast<const unsigned char*>(key.data()), key.size(),
                      reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                      output) != 0) {
    throw std::runtime_error("Failed to create HMAC signature.");
  }

  return base64UrlEncode(std::string(reinterpret_cast<const char*>(output), sizeof(output)));
}

std::string pbkdf2Hash(const std::string& password, const std::string& salt) {
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr) {
    throw std::runtime_error("Unable to initialize PBKDF2 hashing.");
  }

  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  if (mbedtls_md_setup(&context, info, 1) != 0) {
    mbedtls_md_free(&context);
    throw std::runtime_error("Failed to configure PBKDF2 hashing.");
  }

  unsigned char output[32];
  const int result = mbedtls_pkcs5_pbkdf2_hmac(&context,
                                               reinterpret_cast<const unsigned char*>(password.data()),
                                               password.size(),
                                               reinterpret_cast<const unsigned char*>(salt.data()),
                                               salt.size(),
                                               100000,
                                               sizeof(output),
                                               output);
  mbedtls_md_free(&context);

  if (result != 0) {
    throw std::runtime_error("Failed to derive password hash.");
  }

  return bytesToHex(output, sizeof(output));
}

std::string signRs256Base64Url(const std::string& privateKeyPem, const std::string& data) {
  unsigned char hash[32];
  if (mbedtls_sha256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash, 0) != 0) {
    throw std::runtime_error("Failed to hash data for RSA signature.");
  }

  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  if (mbedtls_pk_parse_key(&pk,
                           reinterpret_cast<const unsigned char*>(privateKeyPem.c_str()),
                           privateKeyPem.size() + 1,
                           nullptr,
                           0,
                           mbedtls_ctr_drbg_random,
                           nullptr) != 0) {
    mbedtls_pk_free(&pk);
    throw std::runtime_error("Failed to parse the Firebase private key.");
  }

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctrDrbg;
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctrDrbg);
  const char* personalization = "firebase-service-account";
  if (mbedtls_ctr_drbg_seed(&ctrDrbg,
                            mbedtls_entropy_func,
                            &entropy,
                            reinterpret_cast<const unsigned char*>(personalization),
                            std::strlen(personalization)) != 0) {
    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctrDrbg);
    throw std::runtime_error("Failed to seed RSA signing context.");
  }

  std::vector<unsigned char> signature(mbedtls_pk_get_len(&pk));
  std::size_t signatureLength = 0;
  const int signResult = mbedtls_pk_sign(&pk,
                                         MBEDTLS_MD_SHA256,
                                         hash,
                                         sizeof(hash),
                                         signature.data(),
                                         signature.size(),
                                         &signatureLength,
                                         mbedtls_ctr_drbg_random,
                                         &ctrDrbg);

  mbedtls_pk_free(&pk);
  mbedtls_entropy_free(&entropy);
  mbedtls_ctr_drbg_free(&ctrDrbg);

  if (signResult != 0) {
    throw std::runtime_error("Failed to sign Firebase JWT assertion.");
  }

  return base64UrlEncode(std::string(reinterpret_cast<const char*>(signature.data()), signatureLength));
}

std::string urlEncode(const std::string& value) {
  std::ostringstream escaped;
  escaped << std::hex << std::uppercase;
  for (const unsigned char character : value) {
    if ((character >= 'A' && character <= 'Z') ||
        (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9') ||
        character == '-' || character == '_' || character == '.' || character == '~') {
      escaped << character;
    } else {
      escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(character);
    }
  }
  return escaped.str();
}

std::vector<std::vector<std::string>> parseCsv(const std::string& content) {
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string cell;
  bool inQuotes = false;

  for (std::size_t index = 0; index < content.size(); ++index) {
    const char character = content[index];

    if (character == '"') {
      if (inQuotes && index + 1 < content.size() && content[index + 1] == '"') {
        cell.push_back('"');
        ++index;
      } else {
        inQuotes = !inQuotes;
      }
      continue;
    }

    if (character == ',' && !inQuotes) {
      row.push_back(cell);
      cell.clear();
      continue;
    }

    if ((character == '\n' || character == '\r') && !inQuotes) {
      if (character == '\r' && index + 1 < content.size() && content[index + 1] == '\n') {
        ++index;
      }
      row.push_back(cell);
      cell.clear();
      if (!row.empty()) {
        rows.push_back(row);
      }
      row.clear();
      continue;
    }

    cell.push_back(character);
  }

  if (!cell.empty() || !row.empty()) {
    row.push_back(cell);
    rows.push_back(row);
  }

  return rows;
}

std::string csvEscape(const std::string& value) {
  bool requiresQuoting = false;
  for (const char character : value) {
    if (character == ',' || character == '"' || character == '\n' || character == '\r') {
      requiresQuoting = true;
      break;
    }
  }

  std::string escaped = replaceAll(value, "\"", "\"\"");
  if (requiresQuoting) {
    return "\"" + escaped + "\"";
  }
  return escaped;
}

std::string xmlEscape(const std::string& value) {
  std::string escaped = value;
  escaped = replaceAll(escaped, "&", "&amp;");
  escaped = replaceAll(escaped, "<", "&lt;");
  escaped = replaceAll(escaped, ">", "&gt;");
  escaped = replaceAll(escaped, "\"", "&quot;");
  escaped = replaceAll(escaped, "'", "&apos;");
  return escaped;
}

std::string xmlUnescape(const std::string& value) {
  std::string unescaped = value;
  unescaped = replaceAll(unescaped, "&lt;", "<");
  unescaped = replaceAll(unescaped, "&gt;", ">");
  unescaped = replaceAll(unescaped, "&quot;", "\"");
  unescaped = replaceAll(unescaped, "&apos;", "'");
  unescaped = replaceAll(unescaped, "&amp;", "&");
  return unescaped;
}

}  // namespace lead_manager::util
