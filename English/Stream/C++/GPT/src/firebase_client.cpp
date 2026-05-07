#include "firebase_client.hpp"

#include <array>
#include <sstream>
#include <utility>
#include <vector>

#include <windows.h>
#include <ncrypt.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include "utils.hpp"

namespace {

struct UrlParts {
    std::wstring host;
    std::wstring path_and_query;
    INTERNET_PORT port{};
    bool secure{false};
};

std::optional<UrlParts> crack_url(const std::string& url) {
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    auto wide_url = util::utf8_to_wstring(url);
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &components)) {
        return std::nullopt;
    }

    UrlParts parts;
    parts.host.assign(components.lpszHostName, components.dwHostNameLength);
    parts.path_and_query.assign(components.lpszUrlPath, components.dwUrlPathLength);

    if (components.dwExtraInfoLength > 0 && components.lpszExtraInfo != nullptr) {
        parts.path_and_query.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    parts.port = components.nPort;
    parts.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return parts;
}

std::optional<nlohmann::json> safe_parse_json(const std::string& body) {
    const auto json = nlohmann::json::parse(body, nullptr, false);
    if (json.is_discarded()) {
        return std::nullopt;
    }

    return json;
}

std::string firestore_get_string(const nlohmann::json& fields, const std::string& key) {
    if (!fields.contains(key) || !fields.at(key).contains("stringValue")) {
        return {};
    }

    return fields.at(key).at("stringValue").get<std::string>();
}

std::string build_firestore_document_url(const std::string& project_id, const std::string& document_id) {
    return "https://firestore.googleapis.com/v1/projects/" + project_id +
           "/databases/(default)/documents/users/" + document_id;
}

std::optional<std::vector<unsigned char>> decode_pem_to_der(const std::string& pem) {
    DWORD needed = 0;
    if (!CryptStringToBinaryA(pem.c_str(), 0, CRYPT_STRING_BASE64HEADER, nullptr, &needed, nullptr, nullptr)) {
        return std::nullopt;
    }

    std::vector<unsigned char> der(needed);
    if (!CryptStringToBinaryA(pem.c_str(), 0, CRYPT_STRING_BASE64HEADER, der.data(), &needed, nullptr, nullptr)) {
        return std::nullopt;
    }

    der.resize(needed);
    return der;
}

}  // namespace

FirebaseClient::FirebaseClient(AppConfig config)
    : config_(std::move(config)) {}

bool FirebaseClient::initialize(std::string& error) {
    if (!load_service_account(error)) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool FirebaseClient::register_user(const UserRecord& user, std::string& error) {
    if (!initialized_ && !initialize(error)) {
        return false;
    }

    const auto existing = find_user_by_email(user.email, error);
    if (existing.has_value()) {
        error = "A user with this email already exists.";
        return false;
    }

    if (!error.empty()) {
        return false;
    }

    return write_user_document(user, error);
}

std::optional<UserRecord> FirebaseClient::find_user_by_email(const std::string& email, std::string& error) {
    if (!initialized_ && !initialize(error)) {
        return std::nullopt;
    }

    return fetch_user_document(util::hash_email_to_document_id(email), error);
}

bool FirebaseClient::load_service_account(std::string& error) {
    const auto contents = util::read_text_file(config_.firebase_service_account_path, error);
    if (!contents.has_value()) {
        return false;
    }

    const auto json = nlohmann::json::parse(*contents, nullptr, false);
    if (json.is_discarded()) {
        error = "Invalid Firebase service account JSON.";
        return false;
    }

    service_account_.project_id = json.value("project_id", config_.firebase_project_id);
    service_account_.client_email = json.value("client_email", "");
    service_account_.private_key = json.value("private_key", "");
    service_account_.token_uri = json.value("token_uri", "https://oauth2.googleapis.com/token");

    if (service_account_.project_id.empty()) {
        service_account_.project_id = config_.firebase_project_id;
    }

    if (service_account_.project_id.empty() || service_account_.client_email.empty() || service_account_.private_key.empty()) {
        error = "Firebase service account JSON is missing required fields.";
        return false;
    }

    config_.firebase_project_id = service_account_.project_id;
    return true;
}

std::optional<UserRecord> FirebaseClient::fetch_user_document(const std::string& document_id, std::string& error) {
    if (!ensure_access_token(error)) {
        return std::nullopt;
    }

    const auto response = perform_request(
        "GET",
        build_firestore_document_url(config_.firebase_project_id, document_id),
        "",
        {
            {"Authorization", "Bearer " + access_token_}
        },
        error
    );

    if (!response.has_value()) {
        return std::nullopt;
    }

    if (response->status_code == 404) {
        error.clear();
        return std::nullopt;
    }

    if (response->status_code < 200 || response->status_code >= 300) {
        error = "Firebase read failed with status " + std::to_string(response->status_code) + ".";
        return std::nullopt;
    }

    const auto json = safe_parse_json(response->body);
    if (!json.has_value()) {
        error = "Firebase returned invalid JSON.";
        return std::nullopt;
    }

    const auto& fields = json->at("fields");
    const auto role = role_from_string(firestore_get_string(fields, "role"));
    if (!role.has_value()) {
        error = "Stored Firebase role is invalid.";
        return std::nullopt;
    }

    UserRecord user;
    user.id = firestore_get_string(fields, "id");
    user.name = firestore_get_string(fields, "name");
    user.email = firestore_get_string(fields, "email");
    user.role = *role;
    user.password_hash = firestore_get_string(fields, "passwordHash");
    user.password_salt = firestore_get_string(fields, "passwordSalt");
    user.created_at = firestore_get_string(fields, "createdAt");
    return user;
}

bool FirebaseClient::write_user_document(const UserRecord& user, std::string& error) {
    if (!ensure_access_token(error)) {
        return false;
    }

    const nlohmann::json body = {
        {"fields",
         {
             {"id", {{"stringValue", user.id}}},
             {"name", {{"stringValue", user.name}}},
             {"email", {{"stringValue", user.email}}},
             {"role", {{"stringValue", role_to_string(user.role)}}},
             {"passwordHash", {{"stringValue", user.password_hash}}},
             {"passwordSalt", {{"stringValue", user.password_salt}}},
             {"createdAt", {{"stringValue", user.created_at}}}
         }}
    };

    const auto response = perform_request(
        "PATCH",
        build_firestore_document_url(config_.firebase_project_id, util::hash_email_to_document_id(user.email)),
        body.dump(),
        {
            {"Authorization", "Bearer " + access_token_},
            {"Content-Type", "application/json"}
        },
        error
    );

    if (!response.has_value()) {
        return false;
    }

    if (response->status_code < 200 || response->status_code >= 300) {
        error = "Firebase write failed with status " + std::to_string(response->status_code) + ".";
        return false;
    }

    return true;
}

bool FirebaseClient::ensure_access_token(std::string& error) {
    std::scoped_lock lock(token_mutex_);
    const auto now = std::time(nullptr);

    if (!access_token_.empty() && now < access_token_expires_at_ - 60) {
        return true;
    }

    const auto token = request_access_token(error);
    if (!token.has_value()) {
        return false;
    }

    access_token_ = *token;
    access_token_expires_at_ = std::time(nullptr) + 3600;
    return true;
}

std::optional<FirebaseClient::HttpResponse> FirebaseClient::perform_request(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string& error
) {
    const auto parts = crack_url(url);
    if (!parts.has_value()) {
        error = "Invalid request URL.";
        return std::nullopt;
    }

    HINTERNET session = WinHttpOpen(L"StreamMusicService/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        error = "WinHttpOpen failed.";
        return std::nullopt;
    }

    HINTERNET connection = WinHttpConnect(session, parts->host.c_str(), parts->port, 0);
    if (connection == nullptr) {
        WinHttpCloseHandle(session);
        error = "WinHttpConnect failed.";
        return std::nullopt;
    }

    const DWORD flags = parts->secure ? WINHTTP_FLAG_SECURE : 0;
    auto wide_method = util::utf8_to_wstring(method);
    HINTERNET request = WinHttpOpenRequest(
        connection,
        wide_method.c_str(),
        parts->path_and_query.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (request == nullptr) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        error = "WinHttpOpenRequest failed.";
        return std::nullopt;
    }

    std::wstring raw_headers;
    for (const auto& [name, value] : headers) {
        raw_headers += util::utf8_to_wstring(name + ": " + value + "\r\n");
    }

    BOOL sent = WinHttpSendRequest(
        request,
        raw_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : raw_headers.c_str(),
        raw_headers.empty() ? 0 : static_cast<DWORD>(raw_headers.size()),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0
    );

    if (!sent) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        error = "WinHttpSendRequest failed.";
        return std::nullopt;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        error = "WinHttpReceiveResponse failed.";
        return std::nullopt;
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_code_size,
        WINHTTP_NO_HEADER_INDEX
    );

    std::string response_body;
    DWORD available_size = 0;
    do {
        available_size = 0;
        if (!WinHttpQueryDataAvailable(request, &available_size)) {
            error = "WinHttpQueryDataAvailable failed.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        if (available_size == 0) {
            break;
        }

        std::string chunk(available_size, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request, chunk.data(), available_size, &bytes_read)) {
            error = "WinHttpReadData failed.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        chunk.resize(bytes_read);
        response_body += chunk;
    } while (available_size > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return HttpResponse{static_cast<int>(status_code), std::move(response_body)};
}

std::optional<std::string> FirebaseClient::request_access_token(std::string& error) {
    const auto assertion = build_service_account_assertion(error);
    if (!assertion.has_value()) {
        return std::nullopt;
    }

    const std::string body =
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + util::url_encode(*assertion);

    const auto response = perform_request(
        "POST",
        service_account_.token_uri,
        body,
        {
            {"Content-Type", "application/x-www-form-urlencoded"}
        },
        error
    );

    if (!response.has_value()) {
        return std::nullopt;
    }

    if (response->status_code < 200 || response->status_code >= 300) {
        error = "Google OAuth token request failed with status " + std::to_string(response->status_code) + ".";
        return std::nullopt;
    }

    const auto json = safe_parse_json(response->body);
    if (!json.has_value()) {
        error = "OAuth token response is not valid JSON.";
        return std::nullopt;
    }

    if (!json->contains("access_token")) {
        error = "OAuth token response did not contain an access token.";
        return std::nullopt;
    }

    return json->at("access_token").get<std::string>();
}

std::optional<std::string> FirebaseClient::build_service_account_assertion(std::string& error) const {
    const auto issued_at = util::now_unix_timestamp();
    const auto expires_at = issued_at + 3600;

    const nlohmann::json header = {
        {"alg", "RS256"},
        {"typ", "JWT"}
    };

    const nlohmann::json payload = {
        {"iss", service_account_.client_email},
        {"scope", "https://www.googleapis.com/auth/datastore"},
        {"aud", service_account_.token_uri},
        {"iat", issued_at},
        {"exp", expires_at}
    };

    const auto signing_input = util::base64_url_encode(header.dump()) + "." + util::base64_url_encode(payload.dump());
    const auto der = decode_pem_to_der(service_account_.private_key);
    if (!der.has_value()) {
        error = "Could not decode the Firebase private key.";
        return std::nullopt;
    }

    NCRYPT_PROV_HANDLE provider{};
    NCRYPT_KEY_HANDLE key{};

    SECURITY_STATUS status = NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0);
    if (status != ERROR_SUCCESS) {
        error = "NCryptOpenStorageProvider failed.";
        return std::nullopt;
    }

    status = NCryptImportKey(
        provider,
        0,
        NCRYPT_PKCS8_PRIVATE_KEY_BLOB,
        nullptr,
        &key,
        const_cast<PBYTE>(der->data()),
        static_cast<DWORD>(der->size()),
        0
    );

    if (status != ERROR_SUCCESS) {
        NCryptFreeObject(provider);
        error = "NCryptImportKey failed.";
        return std::nullopt;
    }

    const auto hash = util::sha256_bytes(signing_input);
    BCRYPT_PKCS1_PADDING_INFO padding_info{BCRYPT_SHA256_ALGORITHM};
    DWORD signature_size = 0;

    status = NCryptSignHash(
        key,
        &padding_info,
        const_cast<PBYTE>(hash.data()),
        static_cast<DWORD>(hash.size()),
        nullptr,
        0,
        &signature_size,
        BCRYPT_PAD_PKCS1
    );

    if (status != ERROR_SUCCESS) {
        NCryptFreeObject(key);
        NCryptFreeObject(provider);
        error = "NCryptSignHash size query failed.";
        return std::nullopt;
    }

    std::vector<unsigned char> signature(signature_size);
    status = NCryptSignHash(
        key,
        &padding_info,
        const_cast<PBYTE>(hash.data()),
        static_cast<DWORD>(hash.size()),
        signature.data(),
        signature_size,
        &signature_size,
        BCRYPT_PAD_PKCS1
    );

    NCryptFreeObject(key);
    NCryptFreeObject(provider);

    if (status != ERROR_SUCCESS) {
        error = "NCryptSignHash failed.";
        return std::nullopt;
    }

    signature.resize(signature_size);
    return signing_input + "." + util::base64_url_encode(signature);
}
