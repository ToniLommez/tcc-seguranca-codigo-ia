#pragma once

#include <ctime>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include "app_config.hpp"
#include "models.hpp"

class FirebaseClient {
  public:
    explicit FirebaseClient(AppConfig config);

    bool initialize(std::string& error);
    bool register_user(const UserRecord& user, std::string& error);
    std::optional<UserRecord> find_user_by_email(const std::string& email, std::string& error);

  private:
    struct ServiceAccount {
        std::string project_id;
        std::string client_email;
        std::string private_key;
        std::string token_uri;
    };

    struct HttpResponse {
        int status_code{};
        std::string body;
    };

    AppConfig config_;
    ServiceAccount service_account_;
    bool initialized_{false};

    std::mutex token_mutex_;
    std::string access_token_;
    std::time_t access_token_expires_at_{};

    bool load_service_account(std::string& error);
    std::optional<UserRecord> fetch_user_document(const std::string& document_id, std::string& error);
    bool write_user_document(const UserRecord& user, std::string& error);

    bool ensure_access_token(std::string& error);
    std::optional<HttpResponse> perform_request(
        const std::string& method,
        const std::string& url,
        const std::string& body,
        const std::vector<std::pair<std::string, std::string>>& headers,
        std::string& error
    );
    std::optional<std::string> request_access_token(std::string& error);

    std::optional<std::string> build_service_account_assertion(std::string& error) const;
};

