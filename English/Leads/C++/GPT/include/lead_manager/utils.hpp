#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace lead_manager::util {

std::string nowUtcIso();
std::string currentDateIso();
std::string trim(const std::string& input);
std::string toLower(const std::string& input);
std::string readFile(const std::filesystem::path& path);
std::string randomHex(std::size_t bytes = 16);
std::string generateId();
std::string base64UrlEncode(const std::string& data);
std::string base64UrlDecode(const std::string& data);
std::string hmacSha256Base64Url(const std::string& key, const std::string& data);
std::string pbkdf2Hash(const std::string& password, const std::string& salt);
std::string signRs256Base64Url(const std::string& privateKeyPem, const std::string& data);
std::string urlEncode(const std::string& value);
std::vector<std::vector<std::string>> parseCsv(const std::string& content);
std::string csvEscape(const std::string& value);
std::string xmlEscape(const std::string& value);
std::string xmlUnescape(const std::string& value);

}  // namespace lead_manager::util

