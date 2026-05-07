#include "utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <windows.h>
#include <bcrypt.h>
#include <rpc.h>

namespace {

std::vector<unsigned char> base64_decode_bytes(std::string input) {
    std::replace(input.begin(), input.end(), '-', '+');
    std::replace(input.begin(), input.end(), '_', '/');
    while (input.size() % 4 != 0) {
        input.push_back('=');
    }

    std::vector<int> table(256, -1);
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t i = 0; i < alphabet.size(); ++i) {
        table[static_cast<unsigned char>(alphabet[i])] = static_cast<int>(i);
    }

    std::vector<unsigned char> output;
    int value = 0;
    int bits = -8;
    for (unsigned char ch : input) {
        if (table[ch] == -1) {
            if (ch == '=') {
                break;
            }
            continue;
        }
        value = (value << 6) + table[ch];
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<unsigned char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return output;
}

}  // namespace

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::string join(const std::vector<std::string>& values, const std::string& delimiter) {
    std::ostringstream output;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << delimiter;
        }
        output << values[i];
    }
    return output.str();
}

std::string read_text_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Nao foi possivel abrir o arquivo: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string now_iso8601_utc() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &time);
    std::ostringstream output;
    output << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string today_iso8601_utc() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &time);
    std::ostringstream output;
    output << std::put_time(&tm, "%Y-%m-%d");
    return output.str();
}

std::string random_id() {
    UUID uuid;
    if (UuidCreate(&uuid) != RPC_S_OK) {
        throw std::runtime_error("Falha ao criar UUID");
    }

    RPC_CSTR output = nullptr;
    if (UuidToStringA(&uuid, &output) != RPC_S_OK) {
        throw std::runtime_error("Falha ao converter UUID");
    }

    std::string id(reinterpret_cast<char*>(output));
    RpcStringFreeA(&output);
    return id;
}

std::string sha256_hex(std::string_view input) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD resultLength = 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        throw std::runtime_error("Falha ao abrir SHA256");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &resultLength,
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao obter tamanho do objeto SHA256");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashLength),
            sizeof(hashLength),
            &resultLength,
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao obter tamanho do hash SHA256");
    }

    std::vector<unsigned char> objectBuffer(objectLength);
    std::vector<unsigned char> digest(hashLength);
    if (BCryptCreateHash(algorithm, &hash, objectBuffer.data(), objectLength, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao criar contexto SHA256");
    }

    if (BCryptHashData(
            hash,
            reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
            static_cast<ULONG>(input.size()),
            0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao alimentar SHA256");
    }

    if (BCryptFinishHash(hash, digest.data(), hashLength, 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao finalizar SHA256");
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned char byte : digest) {
        output << std::setw(2) << static_cast<int>(byte);
    }
    return output.str();
}

std::string base64url_encode(const std::string& input) {
    return base64url_encode(std::vector<unsigned char>(input.begin(), input.end()));
}

std::string base64url_encode(const std::vector<unsigned char>& input) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string output;
    int value = 0;
    int bits = -6;
    for (unsigned char ch : input) {
        value = (value << 8) + ch;
        bits += 8;
        while (bits >= 0) {
            output.push_back(alphabet[(value >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) {
        output.push_back(alphabet[((value << 8) >> (bits + 8)) & 0x3F]);
    }
    return output;
}

std::string base64url_decode_to_string(const std::string& input) {
    const auto bytes = base64_decode_bytes(input);
    return std::string(bytes.begin(), bytes.end());
}

std::string url_encode(const std::string& value) {
    std::ostringstream output;
    output << std::hex << std::uppercase;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            output << ch;
        } else {
            output << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return output.str();
}

std::string normalize_email(const std::string& value) {
    return to_lower(trim(value));
}

std::string user_document_id(const std::string& email) {
    return "user_" + sha256_hex(normalize_email(email));
}

std::string normalize_search_text(const std::string& value) {
    return to_lower(trim(value));
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring output(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), output.data(), size);
    return output;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string output(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), output.data(), size, nullptr, nullptr);
    return output;
}

std::string get_executable_directory() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
        throw std::runtime_error("Falha ao descobrir diretorio do executavel");
    }
    std::filesystem::path path(buffer.data());
    return path.parent_path().string();
}
