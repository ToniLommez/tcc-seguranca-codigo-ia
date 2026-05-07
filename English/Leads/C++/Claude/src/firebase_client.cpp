#include "firebase_client.h"
#include "jwt_utils.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <fstream>
#include <iostream>
#include <sstream>

FirebaseClient::FirebaseClient(const std::string& credentials_path) {
    std::ifstream file(credentials_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open Firebase credentials: " + credentials_path);
    }
    nlohmann::json creds;
    file >> creds;
    project_id_ = creds["project_id"].get<std::string>();
    client_email_ = creds["client_email"].get<std::string>();
    private_key_ = creds["private_key"].get<std::string>();
}

std::string FirebaseClient::http_request(const std::string& method,
                                          const std::string& host,
                                          const std::string& path,
                                          const std::string& body,
                                          const std::string& content_type) {
    httplib::SSLClient cli(host, 443);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(30);
    cli.enable_server_certificate_verification(true);

    httplib::Headers headers;
    if (!access_token_.empty()) {
        headers.emplace("Authorization", "Bearer " + access_token_);
    }

    httplib::Result res = nullptr;
    if (method == "GET") {
        res = cli.Get(path, headers);
    } else if (method == "POST") {
        res = cli.Post(path, headers, body, content_type);
    } else if (method == "PATCH") {
        res = cli.Patch(path, headers, body, content_type);
    } else if (method == "DELETE") {
        res = cli.Delete(path, headers);
    }

    if (!res || res->status < 200 || res->status >= 300) {
        int status = res ? res->status : 0;
        std::string resp_body = res ? res->body : "no response";
        std::cerr << "[Firebase] " << method << " " << path
                  << " -> " << status << ": " << resp_body << std::endl;
        return "";
    }
    return res->body;
}

bool FirebaseClient::authenticate() {
    std::string google_jwt = jwt_utils::create_google_jwt(client_email_, private_key_);

    std::string body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
                       "&assertion=" + google_jwt;

    auto response = http_request("POST", "oauth2.googleapis.com",
                                  "/token", body,
                                  "application/x-www-form-urlencoded");
    if (response.empty()) return false;

    try {
        auto json = nlohmann::json::parse(response);
        access_token_ = json["access_token"].get<std::string>();
        std::cout << "[Firebase] Authenticated successfully" << std::endl;
        return true;
    } catch (...) {
        std::cerr << "[Firebase] Auth parse error" << std::endl;
        return false;
    }
}

std::string FirebaseClient::base_path() const {
    return "/v1/projects/" + project_id_ + "/databases/(default)/documents/";
}

nlohmann::json FirebaseClient::to_firestore_value(const nlohmann::json& value) const {
    if (value.is_string())
        return {{"stringValue", value.get<std::string>()}};
    if (value.is_number_integer())
        return {{"integerValue", std::to_string(value.get<int64_t>())}};
    if (value.is_number_float())
        return {{"doubleValue", value.get<double>()}};
    if (value.is_boolean())
        return {{"booleanValue", value.get<bool>()}};
    if (value.is_null())
        return {{"nullValue", nullptr}};
    return {{"stringValue", value.dump()}};
}

nlohmann::json FirebaseClient::to_firestore_fields(const nlohmann::json& doc) const {
    nlohmann::json fields;
    for (auto& [key, val] : doc.items()) {
        fields[key] = to_firestore_value(val);
    }
    return {{"fields", fields}};
}

nlohmann::json FirebaseClient::from_firestore_value(const nlohmann::json& fv) const {
    if (fv.contains("stringValue"))  return fv["stringValue"];
    if (fv.contains("integerValue")) return std::stoll(fv["integerValue"].get<std::string>());
    if (fv.contains("doubleValue"))  return fv["doubleValue"];
    if (fv.contains("booleanValue")) return fv["booleanValue"];
    if (fv.contains("nullValue"))    return nullptr;
    return nullptr;
}

std::string FirebaseClient::extract_doc_id(const std::string& name) const {
    auto pos = name.rfind('/');
    return (pos != std::string::npos) ? name.substr(pos + 1) : name;
}

nlohmann::json FirebaseClient::from_firestore_doc(const nlohmann::json& doc) const {
    nlohmann::json result;
    if (doc.contains("name")) {
        result["id"] = extract_doc_id(doc["name"].get<std::string>());
    }
    if (doc.contains("fields")) {
        for (auto& [key, val] : doc["fields"].items()) {
            result[key] = from_firestore_value(val);
        }
    }
    return result;
}

std::optional<std::string> FirebaseClient::create_document(const std::string& collection,
                                                            const nlohmann::json& fields) {
    auto body = to_firestore_fields(fields);
    auto path = base_path() + collection;

    auto response = http_request("POST", "firestore.googleapis.com",
                                  path, body.dump());
    if (response.empty()) return std::nullopt;

    try {
        auto json = nlohmann::json::parse(response);
        return extract_doc_id(json["name"].get<std::string>());
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> FirebaseClient::get_document(const std::string& collection,
                                                            const std::string& doc_id) {
    auto path = base_path() + collection + "/" + doc_id;
    auto response = http_request("GET", "firestore.googleapis.com", path);
    if (response.empty()) return std::nullopt;

    try {
        auto json = nlohmann::json::parse(response);
        return from_firestore_doc(json);
    } catch (...) {
        return std::nullopt;
    }
}

bool FirebaseClient::update_document(const std::string& collection,
                                      const std::string& doc_id,
                                      const nlohmann::json& fields) {
    auto body = to_firestore_fields(fields);
    std::string path = base_path() + collection + "/" + doc_id + "?";
    for (auto& [key, _] : fields.items()) {
        path += "updateMask.fieldPaths=" + key + "&";
    }
    if (path.back() == '&') path.pop_back();

    auto response = http_request("PATCH", "firestore.googleapis.com",
                                  path, body.dump());
    return !response.empty();
}

bool FirebaseClient::delete_document(const std::string& collection,
                                      const std::string& doc_id) {
    auto path = base_path() + collection + "/" + doc_id;
    auto response = http_request("DELETE", "firestore.googleapis.com", path);
    return true;
}

FirebaseClient::QueryResult FirebaseClient::list_documents(const std::string& collection,
                                                            int page_size,
                                                            const std::string& page_token,
                                                            const std::string& order_by) {
    QueryResult result;
    std::string path = base_path() + collection + "?pageSize=" + std::to_string(page_size);
    if (!page_token.empty()) path += "&pageToken=" + page_token;
    if (!order_by.empty()) path += "&orderBy=" + order_by;

    auto response = http_request("GET", "firestore.googleapis.com", path);
    if (response.empty()) return result;

    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("documents")) {
            for (auto& doc : json["documents"]) {
                result.documents.push_back(from_firestore_doc(doc));
            }
        }
        if (json.contains("nextPageToken")) {
            result.next_page_token = json["nextPageToken"].get<std::string>();
        }
    } catch (...) {}

    return result;
}

FirebaseClient::QueryResult FirebaseClient::run_query(const std::string& collection,
                                                       const nlohmann::json& structured_query) {
    QueryResult result;
    std::string path = "/v1/projects/" + project_id_ + "/databases/(default)/documents:runQuery";

    nlohmann::json body;
    body["structuredQuery"] = structured_query;

    auto response = http_request("POST", "firestore.googleapis.com",
                                  path, body.dump());
    if (response.empty()) return result;

    try {
        auto json = nlohmann::json::parse(response);
        if (json.is_array()) {
            for (auto& item : json) {
                if (item.contains("document")) {
                    result.documents.push_back(from_firestore_doc(item["document"]));
                }
            }
        }
        result.total_count = (int)result.documents.size();
    } catch (...) {}

    return result;
}

FirebaseClient::QueryResult FirebaseClient::query_by_field(const std::string& collection,
                                                            const std::string& field,
                                                            const std::string& op,
                                                            const std::string& value) {
    nlohmann::json query = {
        {"from", {{{"collectionId", collection}}}},
        {"where", {
            {"fieldFilter", {
                {"field", {{"fieldPath", field}}},
                {"op", op},
                {"value", {{"stringValue", value}}}
            }}
        }}
    };
    return run_query(collection, query);
}
