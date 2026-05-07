#pragma once

#include <string>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <curl/curl.h>

namespace soundstream {

static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

class FirebaseClient {
public:
    explicit FirebaseClient(const std::string& credentials_path) {
        std::ifstream file(credentials_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open Firebase credentials: " + credentials_path);
        }

        nlohmann::json creds;
        file >> creds;

        project_id_ = creds["project_id"].get<std::string>();
        client_email_ = creds["client_email"].get<std::string>();
        private_key_ = creds["private_key"].get<std::string>();
        token_uri_ = creds["token_uri"].get<std::string>();

        curl_global_init(CURL_GLOBAL_DEFAULT);
        refresh_access_token();
    }

    ~FirebaseClient() {
        curl_global_cleanup();
    }

    nlohmann::json create_document(const std::string& collection, const std::string& doc_id,
                                    const nlohmann::json& fields) {
        std::string url = firestore_url(collection) + "?documentId=" + doc_id;
        nlohmann::json body = {{"fields", to_firestore_fields(fields)}};
        return http_request("POST", url, body.dump());
    }

    nlohmann::json get_document(const std::string& collection, const std::string& doc_id) {
        std::string url = firestore_url(collection) + "/" + doc_id;
        auto result = http_request("GET", url);
        if (result.contains("error")) return result;
        return from_firestore_document(result);
    }

    nlohmann::json list_documents(const std::string& collection, int page_size = 300) {
        std::string url = firestore_url(collection) + "?pageSize=" + std::to_string(page_size);
        auto result = http_request("GET", url);

        nlohmann::json docs = nlohmann::json::array();
        if (result.contains("documents")) {
            for (auto& doc : result["documents"]) {
                docs.push_back(from_firestore_document(doc));
            }
        }
        return docs;
    }

    nlohmann::json query_by_field(const std::string& collection, const std::string& field,
                                   const std::string& value) {
        std::string url = "https://firestore.googleapis.com/v1/projects/" + project_id_ +
                          "/databases/(default)/documents:runQuery";

        nlohmann::json body = {
            {"structuredQuery", {
                {"from", {{{"collectionId", collection}}}},
                {"where", {
                    {"fieldFilter", {
                        {"field", {{"fieldPath", field}}},
                        {"op", "EQUAL"},
                        {"value", {{"stringValue", value}}}
                    }}
                }}
            }}
        };

        auto result = http_request("POST", url, body.dump());

        nlohmann::json docs = nlohmann::json::array();
        if (result.is_array()) {
            for (auto& item : result) {
                if (item.contains("document")) {
                    docs.push_back(from_firestore_document(item["document"]));
                }
            }
        }
        return docs;
    }

    nlohmann::json delete_document(const std::string& collection, const std::string& doc_id) {
        std::string url = firestore_url(collection) + "/" + doc_id;
        return http_request("DELETE", url);
    }

private:
    std::string project_id_;
    std::string client_email_;
    std::string private_key_;
    std::string token_uri_;
    std::string access_token_;
    std::chrono::system_clock::time_point token_expiry_;
    std::mutex token_mutex_;

    std::string firestore_url(const std::string& path) {
        return "https://firestore.googleapis.com/v1/projects/" + project_id_ +
               "/databases/(default)/documents/" + path;
    }

    void refresh_access_token() {
        auto now = std::chrono::system_clock::now();

        auto google_jwt = jwt::create()
            .set_issuer(client_email_)
            .set_subject(client_email_)
            .set_audience(token_uri_)
            .set_issued_at(now)
            .set_expires_at(now + std::chrono::hours(1))
            .set_payload_claim("scope", jwt::claim(std::string("https://www.googleapis.com/auth/datastore")))
            .sign(jwt::algorithm::rs256("", private_key_, "", ""));

        std::string post_data = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + google_jwt;

        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to init curl");

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, token_uri_.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("Token request failed: ") + curl_easy_strerror(res));
        }

        auto json_resp = nlohmann::json::parse(response);
        if (!json_resp.contains("access_token")) {
            throw std::runtime_error("No access_token in response: " + response);
        }

        access_token_ = json_resp["access_token"].get<std::string>();
        int expires_in = json_resp.value("expires_in", 3600);
        token_expiry_ = now + std::chrono::seconds(expires_in - 60);

        std::cout << "[Firebase] Access token obtained successfully" << std::endl;
    }

    std::string get_token() {
        std::lock_guard<std::mutex> lock(token_mutex_);
        if (std::chrono::system_clock::now() >= token_expiry_) {
            refresh_access_token();
        }
        return access_token_;
    }

    nlohmann::json http_request(const std::string& method, const std::string& url,
                                 const std::string& body = "") {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to init curl");

        std::string response;
        std::string token = get_token();
        std::string auth_header = "Authorization: Bearer " + token;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, auth_header.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        } else if (method == "PATCH") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return {{"error", curl_easy_strerror(res)}};
        }

        try {
            return nlohmann::json::parse(response);
        } catch (...) {
            return {{"raw_response", response}};
        }
    }

    static nlohmann::json to_firestore_fields(const nlohmann::json& data) {
        nlohmann::json fields;
        for (auto& [key, val] : data.items()) {
            if (val.is_string()) {
                fields[key] = {{"stringValue", val.get<std::string>()}};
            } else if (val.is_number_integer()) {
                fields[key] = {{"integerValue", std::to_string(val.get<int64_t>())}};
            } else if (val.is_boolean()) {
                fields[key] = {{"booleanValue", val.get<bool>()}};
            }
        }
        return fields;
    }

    static nlohmann::json from_firestore_document(const nlohmann::json& doc) {
        nlohmann::json result;

        if (doc.contains("name")) {
            std::string name = doc["name"].get<std::string>();
            auto pos = name.rfind('/');
            if (pos != std::string::npos) {
                result["id"] = name.substr(pos + 1);
            }
        }

        if (doc.contains("fields")) {
            for (auto& [key, val] : doc["fields"].items()) {
                if (val.contains("stringValue")) {
                    result[key] = val["stringValue"].get<std::string>();
                } else if (val.contains("integerValue")) {
                    result[key] = val["integerValue"].get<std::string>();
                } else if (val.contains("booleanValue")) {
                    result[key] = val["booleanValue"].get<bool>();
                }
            }
        }

        return result;
    }
};

} // namespace soundstream
