#include "firebase_client.h"
#include "../utils/jwt_utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <curl/curl.h>

using json = nlohmann::json;

// ---- libcurl write callback --------------------------------------------
static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ---- Constructor -------------------------------------------------------
FirebaseClient::FirebaseClient(const std::string& credentialsPath) {
    std::ifstream f(credentialsPath);
    if (!f.is_open())
        throw std::runtime_error("Cannot open Firebase credentials: " + credentialsPath);

    json creds;
    try { f >> creds; }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Invalid credentials JSON: ") + e.what());
    }

    projectId_          = creds.value("project_id",    "");
    serviceAccountEmail_= creds.value("client_email",  "");
    privateKey_         = creds.value("private_key",   "");
    privateKeyId_       = creds.value("private_key_id","");

    if (projectId_.empty() || serviceAccountEmail_.empty() || privateKey_.empty())
        throw std::runtime_error("Incomplete Firebase credentials");

    curl_global_init(CURL_GLOBAL_ALL);
}

// ---- Access-token management -------------------------------------------
const std::string& FirebaseClient::getAccessToken() {
    using namespace std::chrono;
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    if (accessToken_.empty() || now >= tokenExpiry_ - 60)
        refreshAccessToken();
    return accessToken_;
}

void FirebaseClient::refreshAccessToken() {
    std::string jwt = JwtUtils::createGoogleJwt(
        serviceAccountEmail_, privateKey_, privateKeyId_);

    std::string body =
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
        "&assertion=" + jwt;

    auto [code, resp] = httpPost(
        "https://oauth2.googleapis.com/token", body,
        "application/x-www-form-urlencoded");

    if (code != 200)
        throw std::runtime_error("Token exchange failed (" +
                                 std::to_string(code) + "): " + resp);

    json j = json::parse(resp);
    accessToken_ = j.value("access_token", "");
    int expiresIn = j.value("expires_in", 3600);

    using namespace std::chrono;
    tokenExpiry_ =
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count()
        + expiresIn;
}

// ---- Firestore base URL ------------------------------------------------
std::string FirebaseClient::firestoreBase() const {
    return "https://firestore.googleapis.com/v1/projects/" +
           projectId_ + "/databases/(default)/documents/";
}

// ---- Firestore conversion helpers --------------------------------------
json FirebaseClient::toFirestoreFields(const json& obj) {
    json fields = json::object();
    for (auto& [k, v] : obj.items()) {
        if (v.is_string()) {
            fields[k] = {{"stringValue", v}};
        } else if (v.is_boolean()) {
            fields[k] = {{"booleanValue", v}};
        } else if (v.is_number_integer()) {
            fields[k] = {{"integerValue", std::to_string(v.get<int64_t>())}};
        } else if (v.is_number_float()) {
            fields[k] = {{"doubleValue", v}};
        } else if (v.is_null()) {
            fields[k] = {{"nullValue", nullptr}};
        } else if (v.is_array()) {
            json vals = json::array();
            for (auto& item : v) {
                if (item.is_string())
                    vals.push_back({{"stringValue", item}});
                else if (item.is_object())
                    vals.push_back({{"mapValue", {{"fields", toFirestoreFields(item)}}}});
                else if (item.is_boolean())
                    vals.push_back({{"booleanValue", item}});
                else if (item.is_number_integer())
                    vals.push_back({{"integerValue", std::to_string(item.get<int64_t>())}});
            }
            fields[k] = {{"arrayValue", {{"values", vals}}}};
        } else if (v.is_object()) {
            fields[k] = {{"mapValue", {{"fields", toFirestoreFields(v)}}}};
        }
    }
    return fields;
}

json FirebaseClient::fromFirestoreFields(const json& fields) {
    json obj = json::object();
    if (!fields.is_object()) return obj;

    for (auto& [k, v] : fields.items()) {
        if (!v.is_object()) { obj[k] = nullptr; continue; }
        if (v.contains("stringValue"))   obj[k] = v["stringValue"];
        else if (v.contains("booleanValue")) obj[k] = v["booleanValue"];
        else if (v.contains("integerValue")) {
            try { obj[k] = std::stoll(v["integerValue"].get<std::string>()); }
            catch (...) { obj[k] = 0; }
        } else if (v.contains("doubleValue"))  obj[k] = v["doubleValue"];
        else if (v.contains("nullValue"))      obj[k] = nullptr;
        else if (v.contains("arrayValue")) {
            json arr = json::array();
            if (v["arrayValue"].contains("values")) {
                for (auto& item : v["arrayValue"]["values"]) {
                    if (item.contains("stringValue"))
                        arr.push_back(item["stringValue"]);
                    else if (item.contains("mapValue") &&
                             item["mapValue"].contains("fields"))
                        arr.push_back(fromFirestoreFields(item["mapValue"]["fields"]));
                    else if (item.contains("booleanValue"))
                        arr.push_back(item["booleanValue"]);
                    else if (item.contains("integerValue")) {
                        try { arr.push_back(std::stoll(item["integerValue"].get<std::string>())); }
                        catch (...) { arr.push_back(0); }
                    }
                }
            }
            obj[k] = arr;
        } else if (v.contains("mapValue")) {
            if (v["mapValue"].contains("fields"))
                obj[k] = fromFirestoreFields(v["mapValue"]["fields"]);
            else
                obj[k] = json::object();
        }
    }
    return obj;
}

std::string FirebaseClient::extractDocId(const std::string& name) {
    auto pos = name.rfind('/');
    if (pos == std::string::npos) return name;
    return name.substr(pos + 1);
}

// ---- CRUD operations ---------------------------------------------------
std::string FirebaseClient::createDocument(const std::string& collection,
                                           const json& fields) {
    json body = {{"fields", toFirestoreFields(fields)}};
    std::string url = firestoreBase() + collection;
    auto [code, resp] = httpPost(url, body.dump());
    if (code < 200 || code >= 300)
        throw std::runtime_error("createDocument failed (" +
                                 std::to_string(code) + "): " + resp);
    json j = json::parse(resp);
    return extractDocId(j.value("name", ""));
}

std::optional<json> FirebaseClient::getDocument(const std::string& collection,
                                                  const std::string& docId) {
    std::string url = firestoreBase() + collection + "/" + docId;
    auto [code, resp] = httpGet(url);
    if (code == 404) return std::nullopt;
    if (code < 200 || code >= 300)
        throw std::runtime_error("getDocument failed (" +
                                 std::to_string(code) + "): " + resp);
    json j = json::parse(resp);
    json result = fromFirestoreFields(j.value("fields", json::object()));
    result["id"] = extractDocId(j.value("name", ""));
    return result;
}

bool FirebaseClient::updateDocument(const std::string& collection,
                                     const std::string& docId,
                                     const json& fields) {
    // Build updateMask query params
    std::string url = firestoreBase() + collection + "/" + docId + "?";
    bool first = true;
    for (auto& [k, _] : fields.items()) {
        if (!first) url += "&";
        url += "updateMask.fieldPaths=" + k;
        first = false;
    }
    json body = {{"fields", toFirestoreFields(fields)}};
    auto [code, resp] = httpPatch(url, body.dump());
    return code >= 200 && code < 300;
}

bool FirebaseClient::deleteDocument(const std::string& collection,
                                     const std::string& docId) {
    std::string url = firestoreBase() + collection + "/" + docId;
    auto [code, resp] = httpDelete(url);
    return code >= 200 && code < 300;
}

json FirebaseClient::runQuery(const json& structuredQuery) {
    std::string url = "https://firestore.googleapis.com/v1/projects/" +
                      projectId_ + "/databases/(default)/documents:runQuery";
    json reqBody = {{"structuredQuery", structuredQuery}};
    auto [code, resp] = httpPost(url, reqBody.dump());
    if (code < 200 || code >= 300)
        throw std::runtime_error("runQuery failed (" +
                                 std::to_string(code) + "): " + resp);

    json results = json::array();
    json raw = json::parse(resp);
    for (auto& item : raw) {
        if (!item.contains("document")) continue;
        auto& doc = item["document"];
        json r = fromFirestoreFields(doc.value("fields", json::object()));
        r["id"] = extractDocId(doc.value("name", ""));
        results.push_back(r);
    }
    return results;
}

json FirebaseClient::listDocuments(const std::string& collection,
                                    int pageSize,
                                    const std::string& pageToken) {
    std::string url = firestoreBase() + collection +
                      "?pageSize=" + std::to_string(pageSize);
    if (!pageToken.empty())
        url += "&pageToken=" + pageToken;

    auto [code, resp] = httpGet(url);
    if (code < 200 || code >= 300)
        throw std::runtime_error("listDocuments failed (" +
                                 std::to_string(code) + "): " + resp);

    json raw = json::parse(resp);
    json results = json::array();
    if (raw.contains("documents")) {
        for (auto& doc : raw["documents"]) {
            json r = fromFirestoreFields(doc.value("fields", json::object()));
            r["id"] = extractDocId(doc.value("name", ""));
            results.push_back(r);
        }
    }
    return results;
}

// ---- HTTP helpers (libcurl) -------------------------------------------
static std::pair<int, std::string> curlRequest(
        const std::string& url,
        const std::string& method,
        const std::string& body,
        const std::string& contentType,
        const std::string& authToken) {

    CURL* curl = curl_easy_init();
    if (!curl) return {0, "curl_easy_init failed"};

    std::string respBody;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &respBody);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers,
        ("Content-Type: " + contentType).c_str());
    if (!authToken.empty())
        headers = curl_slist_append(headers,
            ("Authorization: Bearer " + authToken).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST,       1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return {0, std::string("CURL error: ") + curl_easy_strerror(res)};

    return {(int)httpCode, respBody};
}

std::pair<int, std::string> FirebaseClient::httpGet(const std::string& url) {
    return curlRequest(url, "GET", "", "application/json", getAccessToken());
}

std::pair<int, std::string> FirebaseClient::httpPost(const std::string& url,
                                                      const std::string& body,
                                                      const std::string& ct) {
    // For the OAuth2 token endpoint we have no access token yet
    std::string token = (url.find("oauth2.googleapis.com") == std::string::npos)
                        ? getAccessToken() : "";
    return curlRequest(url, "POST", body, ct, token);
}

std::pair<int, std::string> FirebaseClient::httpPatch(const std::string& url,
                                                       const std::string& body) {
    return curlRequest(url, "PATCH", body, "application/json", getAccessToken());
}

std::pair<int, std::string> FirebaseClient::httpDelete(const std::string& url) {
    return curlRequest(url, "DELETE", "", "application/json", getAccessToken());
}
