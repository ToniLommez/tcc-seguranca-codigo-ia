#include "utils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

namespace {
constexpr char kBase64UrlAlphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int decode_base64_url_char(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '-') {
    return 62;
  }
  if (c == '_') {
    return 63;
  }
  return -1;
}
} // namespace

namespace utils {

std::string to_lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string trim(std::string value) {
  const auto not_space = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

bool icontains(const std::string& value, const std::string& needle) {
  return to_lower(value).find(to_lower(needle)) != std::string::npos;
}

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Nao foi possivel abrir: " + path.string());
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& content) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("Nao foi possivel escrever: " + path.string());
  }
  stream << content;
}

void write_binary_file(const std::filesystem::path& path, const std::string& content) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("Nao foi possivel escrever: " + path.string());
  }
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::vector<unsigned char> random_bytes(std::size_t size) {
  std::vector<unsigned char> data(size);
  if (RAND_bytes(data.data(), static_cast<int>(data.size())) != 1) {
    throw std::runtime_error("Falha ao gerar bytes aleatorios com OpenSSL.");
  }
  return data;
}

std::string random_hex(std::size_t byte_count) {
  return hex_encode(random_bytes(byte_count));
}

std::string hex_encode(const std::vector<unsigned char>& data) {
  static constexpr char hex[] = "0123456789abcdef";
  std::string output;
  output.reserve(data.size() * 2);

  for (unsigned char byte : data) {
    output.push_back(hex[byte >> 4]);
    output.push_back(hex[byte & 0x0F]);
  }

  return output;
}

std::vector<unsigned char> hex_decode(const std::string& value) {
  if (value.size() % 2 != 0) {
    throw std::runtime_error("Hex invalido.");
  }

  std::vector<unsigned char> output;
  output.reserve(value.size() / 2);

  for (std::size_t i = 0; i < value.size(); i += 2) {
    const auto byte = static_cast<unsigned char>(std::stoul(value.substr(i, 2), nullptr, 16));
    output.push_back(byte);
  }

  return output;
}

std::string base64_url_encode(const std::string& value) {
  return base64_url_encode(std::vector<unsigned char>(value.begin(), value.end()));
}

std::string base64_url_encode(const std::vector<unsigned char>& value) {
  std::string output;
  std::size_t i = 0;

  while (i + 3 <= value.size()) {
    const std::uint32_t n =
      (static_cast<std::uint32_t>(value[i]) << 16U) |
      (static_cast<std::uint32_t>(value[i + 1]) << 8U) |
      static_cast<std::uint32_t>(value[i + 2]);
    output.push_back(kBase64UrlAlphabet[(n >> 18U) & 63U]);
    output.push_back(kBase64UrlAlphabet[(n >> 12U) & 63U]);
    output.push_back(kBase64UrlAlphabet[(n >> 6U) & 63U]);
    output.push_back(kBase64UrlAlphabet[n & 63U]);
    i += 3;
  }

  const auto rem = value.size() - i;
  if (rem == 1) {
    const std::uint32_t n = static_cast<std::uint32_t>(value[i]) << 16U;
    output.push_back(kBase64UrlAlphabet[(n >> 18U) & 63U]);
    output.push_back(kBase64UrlAlphabet[(n >> 12U) & 63U]);
  } else if (rem == 2) {
    const std::uint32_t n =
      (static_cast<std::uint32_t>(value[i]) << 16U) |
      (static_cast<std::uint32_t>(value[i + 1]) << 8U);
    output.push_back(kBase64UrlAlphabet[(n >> 18U) & 63U]);
    output.push_back(kBase64UrlAlphabet[(n >> 12U) & 63U]);
    output.push_back(kBase64UrlAlphabet[(n >> 6U) & 63U]);
  }

  return output;
}

std::vector<unsigned char> base64_url_decode(const std::string& value) {
  std::vector<unsigned char> output;
  int buffer = 0;
  int bits_collected = 0;

  for (char c : value) {
    if (c == '=') {
      break;
    }

    const int decoded = decode_base64_url_char(c);
    if (decoded < 0) {
      throw std::runtime_error("Base64url invalido.");
    }

    buffer = (buffer << 6) | decoded;
    bits_collected += 6;
    if (bits_collected >= 8) {
      bits_collected -= 8;
      output.push_back(static_cast<unsigned char>((buffer >> bits_collected) & 0xFF));
    }
  }

  return output;
}

std::vector<unsigned char> hmac_sha256(const std::string& secret, const std::string& data) {
  unsigned int length = EVP_MAX_MD_SIZE;
  std::vector<unsigned char> output(length);

  if (HMAC(EVP_sha256(),
           secret.data(),
           static_cast<int>(secret.size()),
           reinterpret_cast<const unsigned char*>(data.data()),
           data.size(),
           output.data(),
           &length) == nullptr) {
    throw std::runtime_error("Falha ao assinar HMAC-SHA256.");
  }

  output.resize(length);
  return output;
}

std::vector<unsigned char> rsa_sha256_sign(const std::string& pem_private_key,
                                           const std::string& data) {
  BIO* bio = BIO_new_mem_buf(pem_private_key.data(), static_cast<int>(pem_private_key.size()));
  if (bio == nullptr) {
    throw std::runtime_error("Falha ao abrir a chave privada.");
  }

  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (pkey == nullptr) {
    throw std::runtime_error("Falha ao ler a chave privada PEM.");
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    EVP_PKEY_free(pkey);
    throw std::runtime_error("Falha ao criar contexto de assinatura.");
  }

  std::vector<unsigned char> signature;
  std::size_t signature_length = 0;

  if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1 ||
      EVP_DigestSignUpdate(ctx, data.data(), data.size()) != 1 ||
      EVP_DigestSignFinal(ctx, nullptr, &signature_length) != 1) {
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    throw std::runtime_error("Falha ao preparar assinatura RSA.");
  }

  signature.resize(signature_length);
  if (EVP_DigestSignFinal(ctx, signature.data(), &signature_length) != 1) {
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    throw std::runtime_error("Falha ao finalizar assinatura RSA.");
  }

  signature.resize(signature_length);
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return signature;
}

std::string url_encode(const std::string& value) {
  std::ostringstream encoded;
  encoded << std::hex << std::uppercase;

  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded << c;
    } else {
      encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
  }

  return encoded.str();
}

std::string utc_now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
  std::tm time_info{};
#ifdef _WIN32
  gmtime_s(&time_info, &raw_time);
#else
  gmtime_r(&raw_time, &time_info);
#endif

  std::ostringstream stream;
  stream << std::put_time(&time_info, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::int64_t unix_now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::system_clock::now().time_since_epoch())
    .count();
}

ParsedUrl parse_url(const std::string& value) {
  static const std::regex re(R"(^(https?)://([^/:]+)(?::(\d+))?(\/.*)?$)",
                             std::regex_constants::icase);
  std::smatch match;
  if (!std::regex_match(value, match, re)) {
    throw std::runtime_error("URL invalida: " + value);
  }

  ParsedUrl parsed;
  parsed.scheme = to_lower(match[1].str());
  parsed.host = match[2].str();
  parsed.port = match[3].matched ? std::stoi(match[3].str()) : (parsed.scheme == "https" ? 443 : 80);
  parsed.path = match[4].matched ? match[4].str() : "/";
  return parsed;
}

void set_json(httplib::Response& res, const nlohmann::json& payload, int status) {
  res.status = status;
  res.set_content(payload.dump(2), "application/json; charset=utf-8");
}

void set_error(httplib::Response& res, int status, const std::string& message) {
  set_json(res, {{"error", message}}, status);
}

} // namespace utils
