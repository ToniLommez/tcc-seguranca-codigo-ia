#include "security.hpp"

#include <chrono>
#include <stdexcept>
#include <vector>

#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>

#include "../external/json.hpp"
#include "utils.hpp"

namespace {

std::vector<unsigned char> decode_pem_to_der(const std::string& pem) {
    DWORD outputLength = 0;
    if (!CryptStringToBinaryA(
            pem.c_str(),
            static_cast<DWORD>(pem.size()),
            CRYPT_STRING_BASE64HEADER,
            nullptr,
            &outputLength,
            nullptr,
            nullptr)) {
        throw std::runtime_error("Falha ao decodificar a chave privada PEM");
    }

    std::vector<unsigned char> der(outputLength);
    if (!CryptStringToBinaryA(
            pem.c_str(),
            static_cast<DWORD>(pem.size()),
            CRYPT_STRING_BASE64HEADER,
            der.data(),
            &outputLength,
            nullptr,
            nullptr)) {
        throw std::runtime_error("Falha ao converter PEM em DER");
    }
    der.resize(outputLength);
    return der;
}

std::vector<unsigned char> sha256_bytes(std::string_view input) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0;
    DWORD resultLength = 0;
    DWORD hashLength = 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        throw std::runtime_error("Falha ao abrir o algoritmo SHA256");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &resultLength,
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao obter BCRYPT_OBJECT_LENGTH");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashLength),
            sizeof(hashLength),
            &resultLength,
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao obter BCRYPT_HASH_LENGTH");
    }

    std::vector<unsigned char> objectBuffer(objectLength);
    std::vector<unsigned char> digest(hashLength);

    if (BCryptCreateHash(algorithm, &hash, objectBuffer.data(), objectLength, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao criar hash SHA256");
    }

    if (BCryptHashData(
            hash,
            reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
            static_cast<ULONG>(input.size()),
            0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao alimentar hash SHA256");
    }

    if (BCryptFinishHash(hash, digest.data(), hashLength, 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao finalizar hash SHA256");
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return digest;
}

std::vector<unsigned char> hmac_sha256_bytes(std::string_view key, std::string_view message) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD resultLength = 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        throw std::runtime_error("Falha ao abrir HMAC SHA256");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &resultLength,
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao ler BCRYPT_OBJECT_LENGTH do HMAC");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashLength),
            sizeof(hashLength),
            &resultLength,
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao ler BCRYPT_HASH_LENGTH do HMAC");
    }

    std::vector<unsigned char> objectBuffer(objectLength);
    std::vector<unsigned char> digest(hashLength);

    if (BCryptCreateHash(
            algorithm,
            &hash,
            objectBuffer.data(),
            objectLength,
            reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
            static_cast<ULONG>(key.size()),
            0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao criar HMAC");
    }

    if (BCryptHashData(
            hash,
            reinterpret_cast<PUCHAR>(const_cast<char*>(message.data())),
            static_cast<ULONG>(message.size()),
            0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao alimentar HMAC");
    }

    if (BCryptFinishHash(hash, digest.data(), hashLength, 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Falha ao finalizar HMAC");
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return digest;
}

std::vector<unsigned char> pbkdf2_sha256(const std::string& password, const std::vector<unsigned char>& salt, unsigned long long rounds) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        throw std::runtime_error("Falha ao abrir algoritmo para PBKDF2");
    }

    std::vector<unsigned char> derived(32);
    const auto status = BCryptDeriveKeyPBKDF2(
        algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(salt.data()),
        static_cast<ULONG>(salt.size()),
        rounds,
        derived.data(),
        static_cast<ULONG>(derived.size()),
        0
    );

    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status != 0) {
        throw std::runtime_error("Falha ao derivar PBKDF2");
    }
    return derived;
}

long long unix_now() {
    const auto now = std::chrono::system_clock::now();
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
}

std::vector<unsigned char> random_bytes(std::size_t count) {
    std::vector<unsigned char> bytes(count);
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("Falha ao gerar bytes aleatorios");
    }
    return bytes;
}

std::string to_hex(const std::vector<unsigned char>& bytes) {
    static const char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        output.push_back(digits[(byte >> 4) & 0x0F]);
        output.push_back(digits[byte & 0x0F]);
    }
    return output;
}

std::vector<unsigned char> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Hex invalido");
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

}  // namespace

ServiceAccountCredentials load_service_account(const std::string& path) {
    const auto raw = read_text_file(path);
    const auto parsed = json::parse(raw);

    ServiceAccountCredentials credentials;
    credentials.projectId = parsed.value("project_id", "");
    credentials.privateKeyId = parsed.value("private_key_id", "");
    credentials.privateKeyPem = parsed.value("private_key", "");
    credentials.clientEmail = parsed.value("client_email", "");
    credentials.tokenUri = parsed.value("token_uri", "https://oauth2.googleapis.com/token");

    if (credentials.projectId.empty() || credentials.privateKeyPem.empty() || credentials.clientEmail.empty()) {
        throw std::runtime_error("Service account invalida: campos obrigatorios ausentes");
    }

    return credentials;
}

std::string hash_password(const std::string& password) {
    const auto salt = random_bytes(16);
    const auto derived = pbkdf2_sha256(password, salt, 120000);
    return "pbkdf2_sha256$120000$" + to_hex(salt) + "$" + to_hex(derived);
}

bool verify_password(const std::string& password, const std::string& encodedHash) {
    const auto parts = split(encodedHash, '$');
    if (parts.size() != 4 || parts[0] != "pbkdf2_sha256") {
        return false;
    }

    const auto rounds = static_cast<unsigned long long>(std::stoull(parts[1]));
    const auto salt = from_hex(parts[2]);
    const auto expected = from_hex(parts[3]);
    const auto actual = pbkdf2_sha256(password, salt, rounds);
    if (actual.size() != expected.size()) {
        return false;
    }

    unsigned char diff = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        diff |= actual[i] ^ expected[i];
    }
    return diff == 0;
}

std::string create_app_jwt(const AppConfig& config, const AuthUser& user) {
    const json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };

    const auto now = unix_now();
    const json payload = {
        {"iss", config.jwtIssuer},
        {"sub", user.id},
        {"email", user.email},
        {"name", user.name},
        {"iat", now},
        {"exp", now + 60 * 60 * 12}
    };

    const auto signingInput = base64url_encode(header.dump()) + "." + base64url_encode(payload.dump());
    const auto signature = hmac_sha256_bytes(config.jwtSecret, signingInput);
    return signingInput + "." + base64url_encode(signature);
}

std::optional<JwtPayload> verify_app_jwt(const AppConfig& config, const std::string& token) {
    const auto parts = split(token, '.');
    if (parts.size() != 3) {
        return std::nullopt;
    }

    const auto signingInput = parts[0] + "." + parts[1];
    const auto expected = base64url_encode(hmac_sha256_bytes(config.jwtSecret, signingInput));
    if (expected != parts[2]) {
        return std::nullopt;
    }

    try {
        const auto payloadJson = json::parse(base64url_decode_to_string(parts[1]));
        const auto exp = payloadJson.value("exp", 0LL);
        if (exp <= unix_now()) {
            return std::nullopt;
        }

        JwtPayload payload;
        payload.userId = payloadJson.value("sub", "");
        payload.email = payloadJson.value("email", "");
        payload.name = payloadJson.value("name", "");
        payload.exp = exp;
        if (payload.userId.empty() || payload.email.empty()) {
            return std::nullopt;
        }
        return payload;
    } catch (...) {
        return std::nullopt;
    }
}

std::string sign_service_account_jwt(
    const ServiceAccountCredentials& credentials,
    const std::string& audience,
    const json& claims
) {
    const auto now = unix_now();
    json payload = claims;
    payload["iss"] = credentials.clientEmail;
    payload["aud"] = audience;
    payload["iat"] = now;
    payload["exp"] = now + 3600;

    const json header = {
        {"alg", "RS256"},
        {"typ", "JWT"},
        {"kid", credentials.privateKeyId}
    };

    const auto signingInput = base64url_encode(header.dump()) + "." + base64url_encode(payload.dump());
    const auto digest = sha256_bytes(signingInput);
    const auto der = decode_pem_to_der(credentials.privateKeyPem);

    NCRYPT_PROV_HANDLE provider = 0;
    NCRYPT_KEY_HANDLE key = 0;

    if (NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0) != 0) {
        throw std::runtime_error("Falha ao abrir storage provider");
    }

    if (NCryptImportKey(
            provider,
            0,
            NCRYPT_PKCS8_PRIVATE_KEY_BLOB,
            nullptr,
            &key,
            const_cast<PBYTE>(der.data()),
            static_cast<DWORD>(der.size()),
            0) != 0) {
        NCryptFreeObject(provider);
        throw std::runtime_error("Falha ao importar chave privada PKCS8");
    }

    BCRYPT_PKCS1_PADDING_INFO paddingInfo{};
    paddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    DWORD signatureSize = 0;
    if (NCryptSignHash(
            key,
            &paddingInfo,
            const_cast<PBYTE>(digest.data()),
            static_cast<DWORD>(digest.size()),
            nullptr,
            0,
            &signatureSize,
            BCRYPT_PAD_PKCS1) != 0) {
        NCryptFreeObject(key);
        NCryptFreeObject(provider);
        throw std::runtime_error("Falha ao consultar assinatura RSA");
    }

    std::vector<unsigned char> signature(signatureSize);
    if (NCryptSignHash(
            key,
            &paddingInfo,
            const_cast<PBYTE>(digest.data()),
            static_cast<DWORD>(digest.size()),
            signature.data(),
            static_cast<DWORD>(signature.size()),
            &signatureSize,
            BCRYPT_PAD_PKCS1) != 0) {
        NCryptFreeObject(key);
        NCryptFreeObject(provider);
        throw std::runtime_error("Falha ao assinar JWT do service account");
    }

    signature.resize(signatureSize);
    NCryptFreeObject(key);
    NCryptFreeObject(provider);

    return signingInput + "." + base64url_encode(signature);
}
