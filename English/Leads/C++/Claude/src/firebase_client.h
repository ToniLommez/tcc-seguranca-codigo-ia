#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

class FirebaseClient {
public:
    FirebaseClient(const std::string& credentials_path);

    bool authenticate();

    std::optional<std::string> create_document(const std::string& collection,
                                                const nlohmann::json& fields);

    std::optional<nlohmann::json> get_document(const std::string& collection,
                                                const std::string& doc_id);

    bool update_document(const std::string& collection,
                         const std::string& doc_id,
                         const nlohmann::json& fields);

    bool delete_document(const std::string& collection,
                         const std::string& doc_id);

    struct QueryResult {
        std::vector<nlohmann::json> documents;
        std::string next_page_token;
        int total_count = 0;
    };

    QueryResult list_documents(const std::string& collection,
                               int page_size = 20,
                               const std::string& page_token = "",
                               const std::string& order_by = "");

    QueryResult run_query(const std::string& collection,
                          const nlohmann::json& structured_query);

    QueryResult query_by_field(const std::string& collection,
                               const std::string& field,
                               const std::string& op,
                               const std::string& value);

    std::string project_id() const { return project_id_; }

private:
    std::string project_id_;
    std::string client_email_;
    std::string private_key_;
    std::string access_token_;

    std::string base_path() const;
    nlohmann::json to_firestore_value(const nlohmann::json& value) const;
    nlohmann::json to_firestore_fields(const nlohmann::json& doc) const;
    nlohmann::json from_firestore_value(const nlohmann::json& fv) const;
    nlohmann::json from_firestore_doc(const nlohmann::json& doc) const;
    std::string extract_doc_id(const std::string& name) const;

    std::string http_request(const std::string& method,
                             const std::string& host,
                             const std::string& path,
                             const std::string& body = "",
                             const std::string& content_type = "application/json");
};
