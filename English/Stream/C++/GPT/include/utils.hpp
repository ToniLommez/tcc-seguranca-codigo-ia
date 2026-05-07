#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace util {

std::string trim_copy(const std::string& value);
std::string lowercase_copy(std::string value);
bool iequals(const std::string& left, const std::string& right);
bool icontains(const std::string& text, const std::string& needle);

bool file_exists(const std::filesystem::path& path);
bool ensure_directory(const std::filesystem::path& path, std::string& error);
std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error);
bool write_text_file(const std::filesystem::path& path, const std::string& contents, std::string& error);
bool write_binary_file(const std::filesystem::path& path, const std::string& contents, std::string& error);
std::optional<std::string> read_binary_range(
    const std::filesystem::path& path,
    std::uint64_t start,
    std::uint64_t length,
    std::string& error
);
std::uint64_t file_size_or_zero(const std::filesystem::path& path);

std::wstring utf8_to_wstring(const std::string& value);
std::string wstring_to_utf8(const std::wstring& value);

std::string current_utc_timestamp();
std::int64_t now_unix_timestamp();
std::string generate_id();
std::string sanitize_filename(const std::string& filename);

std::vector<unsigned char> random_bytes(std::size_t size);
std::vector<unsigned char> sha256_bytes(const std::string& text);
std::string sha256_hex(const std::string& text);
std::vector<unsigned char> hmac_sha256(const std::string& secret, const std::string& message);
bool constant_time_equals(const std::string& left, const std::string& right);

std::string base64_url_encode(const std::vector<unsigned char>& data);
std::string base64_url_encode(const std::string& data);
std::string base64_url_decode_to_string(const std::string& encoded);
std::vector<unsigned char> base64_url_decode(const std::string& encoded);

std::string hash_email_to_document_id(const std::string& email);
std::string url_encode(const std::string& value);

}  // namespace util

