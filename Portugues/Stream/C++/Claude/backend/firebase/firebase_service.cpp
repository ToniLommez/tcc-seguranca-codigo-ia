#include "firebase_service.h"
#include "auth/jwt_helper.h"
#include <curl/curl.h>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <iostream>

// --------------------------------------------------------------------------
// cURL write callback
// --------------------------------------------------------------------------
static size_t curlWriteCb(void* contents, size_t size, size_t nmemb, std::string* out) {
    out->append(reinterpret_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

static std::string curlPost(const std::string& url,
                             const std::string& body,
                             const std::string& contentType,
                             const std::vector<std::string>& headers = {}) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    struct curl_slist* hlist = nullptr;
    hlist = curl_slist_append(hlist, ("Content-Type: " + contentType).c_str());
    for (auto& h : headers) hlist = curl_slist_append(hlist, h.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(res));
    return response;
}

static std::string curlGet(const std::string& url,
                            const std::vector<std::string>& headers = {}) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    struct curl_slist* hlist = nullptr;
    for (auto& h : headers) hlist = curl_slist_append(hlist, h.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(res));
    return response;
}

// --------------------------------------------------------------------------
// Constructor – reads credentials JSON
// --------------------------------------------------------------------------
FirebaseService::FirebaseService(const std::string& credentialsPath) {
    std::ifstream f(credentialsPath);
    if (!f.is_open()) throw std::runtime_error("Cannot open Firebase credentials: " + credentialsPath);

    nlohmann::json creds;
    f >> creds;

    m_clientEmail = creds["client_email"].get<std::string>();
    m_privateKey  = creds["private_key"].get<std::string>();
    m_projectId   = creds["project_id"].get<std::string>();
}

// --------------------------------------------------------------------------
// OAuth2 access token (cached, refreshed when close to expiry)
// --------------------------------------------------------------------------
std::string FirebaseService::getAccessToken() {
    long long now = (long long)std::time(nullptr);
    if (!m_accessToken.empty() && now < m_tokenExpiry - 60) {
        return m_accessToken;
    }

    const std::string scope    = "https://www.googleapis.com/auth/cloud-platform "
                                 "https://www.googleapis.com/auth/datastore";
    const std::string audience = "https://oauth2.googleapis.com/token";

    std::string jwt = JwtHelper::createServiceAccountJwt(
        m_clientEmail, m_privateKey, scope, audience);

    std::string body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
                       "&assertion=" + jwt;

    std::string resp = curlPost("https://oauth2.googleapis.com/token", body,
                                "application/x-www-form-urlencoded");

    auto j = nlohmann::json::parse(resp);
    if (!j.contains("access_token")) {
        throw std::runtime_error("Failed to obtain access token: " + resp);
    }

    m_accessToken  = j["access_token"].get<std::string>();
    m_tokenExpiry  = now + j.value("expires_in", 3600);
    return m_accessToken;
}

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------
std::string FirebaseService::emailToDocId(const std::string& email) {
    std::string id = email;
    for (auto& c : id) {
        if (c == '@') c = '_';
        else if (c == '.') c = '_';
        else if (c == '+') c = '_';
    }
    return id;
}

nlohmann::json FirebaseService::userToFirestoreFields(const UserDocument& u) {
    return {
        {"fields", {
            {"name",          {{"stringValue", u.name}}},
            {"email",         {{"stringValue", u.email}}},
            {"password_hash", {{"stringValue", u.passwordHash}}},
            {"password_salt", {{"stringValue", u.passwordSalt}}},
            {"type",          {{"stringValue", u.type}}}
        }}
    };
}

UserDocument FirebaseService::firestoreDocToUser(const nlohmann::json& doc, const std::string& docId) {
    UserDocument u;
    u.id = docId;
    const auto& fields = doc["fields"];
    if (fields.contains("name"))          u.name         = fields["name"]["stringValue"].get<std::string>();
    if (fields.contains("email"))         u.email        = fields["email"]["stringValue"].get<std::string>();
    if (fields.contains("password_hash")) u.passwordHash = fields["password_hash"]["stringValue"].get<std::string>();
    if (fields.contains("password_salt")) u.passwordSalt = fields["password_salt"]["stringValue"].get<std::string>();
    if (fields.contains("type"))          u.type         = fields["type"]["stringValue"].get<std::string>();
    return u;
}

// --------------------------------------------------------------------------
// Firestore REST helpers
// --------------------------------------------------------------------------
std::string FirebaseService::firestorePost(const std::string& path,
                                           const std::string& body,
                                           const std::string& token) {
    std::string url = "https://firestore.googleapis.com/v1/" + path;
    return curlPost(url, body, "application/json",
                    {"Authorization: Bearer " + token});
}

std::string FirebaseService::firestoreGet(const std::string& path,
                                          const std::string& token) {
    std::string url = "https://firestore.googleapis.com/v1/" + path;
    return curlGet(url, {"Authorization: Bearer " + token});
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------
bool FirebaseService::createUser(const UserDocument& user) {
    try {
        auto token  = getAccessToken();
        auto docId  = emailToDocId(user.email);
        auto fields = userToFirestoreFields(user);

        std::string path = "projects/" + m_projectId +
                           "/databases/(default)/documents/users?documentId=" + docId;
        std::string resp = firestorePost(path, fields.dump(), token);

        auto j = nlohmann::json::parse(resp);
        return j.contains("name");
    } catch (const std::exception& e) {
        std::cerr << "[Firebase] createUser error: " << e.what() << "\n";
        return false;
    }
}

std::optional<UserDocument> FirebaseService::getUserByEmail(const std::string& email) {
    try {
        auto token = getAccessToken();
        auto docId = emailToDocId(email);
        std::string path = "projects/" + m_projectId +
                           "/databases/(default)/documents/users/" + docId;
        std::string resp = firestoreGet(path, token);

        auto j = nlohmann::json::parse(resp);
        if (!j.contains("fields")) return std::nullopt;

        return firestoreDocToUser(j, docId);
    } catch (const std::exception& e) {
        std::cerr << "[Firebase] getUserByEmail error: " << e.what() << "\n";
        return std::nullopt;
    }
}
