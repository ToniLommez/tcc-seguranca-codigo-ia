#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../external/json.hpp"

using json = nlohmann::json;

std::string to_lower(std::string value);
std::string trim(std::string value);
std::vector<std::string> split(const std::string& value, char delimiter);
std::string join(const std::vector<std::string>& values, const std::string& delimiter);
std::string read_text_file(const std::string& path);
std::string now_iso8601_utc();
std::string today_iso8601_utc();
std::string random_id();
std::string sha256_hex(std::string_view input);
std::string base64url_encode(const std::string& input);
std::string base64url_encode(const std::vector<unsigned char>& input);
std::string base64url_decode_to_string(const std::string& input);
std::string url_encode(const std::string& value);
std::string normalize_email(const std::string& value);
std::string user_document_id(const std::string& email);
std::string normalize_search_text(const std::string& value);
std::wstring utf8_to_wide(const std::string& value);
std::string wide_to_utf8(const std::wstring& value);
std::string get_executable_directory();

