#include "utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

namespace util {

namespace {

std::string join_path_errors(const std::filesystem::filesystem_error& error) {
    return error.code().message() + ": " + error.path1().string();
}

std::string base64_url_from_standard(std::string value) {
    std::replace(value.begin(), value.end(), '+', '-');
    std::replace(value.begin(), value.end(), '/', '_');

    while (!value.empty() && value.back() == '=') {
        value.pop_back();
    }

    return value;
}

std::string base64_standard_from_url(std::string value) {
    std::replace(value.begin(), value.end(), '-', '+');
    std::replace(value.begin(), value.end(), '_', '/');

    const auto remainder = value.size() % 4;
    if (remainder != 0U) {
        value.append(4U - remainder, '=');
    }

    return value;
}

std::string to_hex(const unsigned char* data, std::size_t length) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');

    for (std::size_t index = 0; index < length; ++index) {
        stream << std::setw(2) << static_cast<int>(data[index]);
    }

    return stream.str();
}

}  // namespace

std::string trim_copy(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();

    if (first >= last) {
        return {};
    }

    return std::string(first, last);
}

std::string lowercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool iequals(const std::string& left, const std::string& right) {
    return lowercase_copy(left) == lowercase_copy(right);
}

bool icontains(const std::string& text, const std::string& needle) {
    return lowercase_copy(text).find(lowercase_copy(needle)) != std::string::npos;
}

bool file_exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

bool ensure_directory(const std::filesystem::path& path, std::string& error) {
    std::error_code fs_error;

    if (std::filesystem::exists(path, fs_error)) {
        return true;
    }

    if (!std::filesystem::create_directories(path, fs_error)) {
        error = fs_error.message();
        return false;
    }

    return true;
}

std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Could not open file: " + path.string();
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool write_text_file(const std::filesystem::path& path, const std::string& contents, std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "Could not write file: " + path.string();
        return false;
    }

    stream << contents;
    return true;
}

bool write_binary_file(const std::filesystem::path& path, const std::string& contents, std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "Could not write file: " + path.string();
        return false;
    }

    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return true;
}

std::optional<std::string> read_binary_range(
    const std::filesystem::path& path,
    std::uint64_t start,
    std::uint64_t length,
    std::string& error
) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Could not open audio file: " + path.string();
        return std::nullopt;
    }

    stream.seekg(static_cast<std::streamoff>(start), std::ios::beg);

    std::string buffer(length, '\0');
    stream.read(buffer.data(), static_cast<std::streamsize>(length));
    buffer.resize(static_cast<std::size_t>(stream.gcount()));
    return buffer;
}

std::uint64_t file_size_or_zero(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) ? std::filesystem::file_size(path, error) : 0U;
}

std::wstring utf8_to_wstring(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring converted(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), converted.data(), length);
    return converted;
}

std::string wstring_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string converted(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), converted.data(), length, nullptr, nullptr);
    return converted;
}

std::string current_utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const auto epoch = seconds.time_since_epoch().count();
    const std::time_t time = static_cast<std::time_t>(epoch);

    std::tm utc{};
    gmtime_s(&utc, &time);

    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::int64_t now_unix_timestamp() {
    return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::string generate_id() {
    constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    auto bytes = random_bytes(12);
    std::string value;
    value.reserve(24);

    for (unsigned char byte : bytes) {
        value.push_back(alphabet[byte % 36]);
        value.push_back(alphabet[(byte / 36) % 36]);
    }

    return value;
}

std::string sanitize_filename(const std::string& filename) {
    std::string clean;
    clean.reserve(filename.size());

    for (unsigned char ch : filename) {
        if (std::isalnum(ch) || ch == '.' || ch == '-' || ch == '_') {
            clean.push_back(static_cast<char>(ch));
        } else if (std::isspace(ch)) {
            clean.push_back('_');
        }
    }

    if (clean.empty()) {
        clean = "file";
    }

    return clean;
}

std::vector<unsigned char> random_bytes(std::size_t size) {
    std::vector<unsigned char> buffer(size);
    BCryptGenRandom(nullptr, buffer.data(), static_cast<ULONG>(buffer.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return buffer;
}

std::vector<unsigned char> sha256_bytes(const std::string& text) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash_handle{};
    DWORD object_size{};
    DWORD bytes_copied{};
    DWORD hash_size{};

    BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &bytes_copied, 0);
    BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &bytes_copied, 0);

    std::vector<unsigned char> hash_object(object_size);
    std::vector<unsigned char> hash(hash_size);

    BCryptCreateHash(algorithm, &hash_handle, hash_object.data(), static_cast<ULONG>(hash_object.size()), nullptr, 0, 0);
    BCryptHashData(hash_handle, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()), 0);
    BCryptFinishHash(hash_handle, hash.data(), static_cast<ULONG>(hash.size()), 0);

    BCryptDestroyHash(hash_handle);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    return hash;
}

std::string sha256_hex(const std::string& text) {
    const auto hash = sha256_bytes(text);
    return to_hex(hash.data(), hash.size());
}

std::vector<unsigned char> hmac_sha256(const std::string& secret, const std::string& message) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash_handle{};
    DWORD object_size{};
    DWORD bytes_copied{};
    DWORD hash_size{};

    BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &bytes_copied, 0);
    BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &bytes_copied, 0);

    std::vector<unsigned char> hash_object(object_size);
    std::vector<unsigned char> hash(hash_size);

    BCryptCreateHash(
        algorithm,
        &hash_handle,
        hash_object.data(),
        static_cast<ULONG>(hash_object.size()),
        reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),
        static_cast<ULONG>(secret.size()),
        0
    );

    BCryptHashData(hash_handle, reinterpret_cast<PUCHAR>(const_cast<char*>(message.data())), static_cast<ULONG>(message.size()), 0);
    BCryptFinishHash(hash_handle, hash.data(), static_cast<ULONG>(hash.size()), 0);

    BCryptDestroyHash(hash_handle);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    return hash;
}

bool constant_time_equals(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }

    unsigned char diff = 0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        diff |= static_cast<unsigned char>(left[index] ^ right[index]);
    }

    return diff == 0;
}

std::string base64_url_encode(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return {};
    }

    DWORD needed = 0;
    CryptBinaryToStringA(
        data.data(),
        static_cast<DWORD>(data.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        nullptr,
        &needed
    );

    std::string encoded(static_cast<std::size_t>(needed), '\0');
    CryptBinaryToStringA(
        data.data(),
        static_cast<DWORD>(data.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        encoded.data(),
        &needed
    );
    encoded.resize(needed > 0 ? needed - 1U : 0U);
    return base64_url_from_standard(encoded);
}

std::string base64_url_encode(const std::string& data) {
    return base64_url_encode(std::vector<unsigned char>(data.begin(), data.end()));
}

std::vector<unsigned char> base64_url_decode(const std::string& encoded) {
    const auto standard = base64_standard_from_url(encoded);

    DWORD needed = 0;
    CryptStringToBinaryA(standard.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &needed, nullptr, nullptr);

    std::vector<unsigned char> decoded(needed);
    CryptStringToBinaryA(standard.c_str(), 0, CRYPT_STRING_BASE64, decoded.data(), &needed, nullptr, nullptr);
    decoded.resize(needed);
    return decoded;
}

std::string base64_url_decode_to_string(const std::string& encoded) {
    const auto decoded = base64_url_decode(encoded);
    return std::string(decoded.begin(), decoded.end());
}

std::string hash_email_to_document_id(const std::string& email) {
    return sha256_hex(lowercase_copy(trim_copy(email)));
}

std::string url_encode(const std::string& value) {
    std::ostringstream stream;
    stream << std::hex << std::uppercase;

    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            stream << ch;
        } else {
            stream << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }

    return stream.str();
}

}  // namespace util
