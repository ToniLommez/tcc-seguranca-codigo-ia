#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace utils {

struct ParsedUrl {
  std::string scheme;
  std::string host;
  int port = 443;
  std::string path;
};

std::string to_lower(std::string value);
std::string trim(std::string value);
bool icontains(const std::string& value, const std::string& needle);

std::string read_text_file(const std::filesystem::path& path);
void write_text_file(const std::filesystem::path& path, const std::string& content);
void write_binary_file(const std::filesystem::path& path, const std::string& content);

std::vector<unsigned char> random_bytes(std::size_t size);
std::string random_hex(std::size_t byte_count);
std::string hex_encode(const std::vector<unsigned char>& data);
std::vector<unsigned char> hex_decode(const std::string& value);

std::string base64_url_encode(const std::string& value);
std::string base64_url_encode(const std::vector<unsigned char>& value);
std::vector<unsigned char> base64_url_decode(const std::string& value);

std::vector<unsigned char> hmac_sha256(const std::string& secret, const std::string& data);
std::vector<unsigned char> rsa_sha256_sign(const std::string& pem_private_key,
                                           const std::string& data);

std::string url_encode(const std::string& value);
std::string utc_now_iso8601();
std::int64_t unix_now();

ParsedUrl parse_url(const std::string& value);

void set_json(httplib::Response& res, const nlohmann::json& payload, int status = 200);
void set_error(httplib::Response& res, int status, const std::string& message);

} // namespace utils
