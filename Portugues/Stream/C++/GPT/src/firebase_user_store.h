#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "app_config.h"
#include "models.h"

class FirebaseUserStore {
public:
  explicit FirebaseUserStore(AppConfig config);

  std::optional<UserRecord> find_by_email(const std::string& email);
  bool create_user(const UserRecord& user, std::string& error_message);

private:
  struct ServiceAccount {
    std::string project_id;
    std::string client_email;
    std::string private_key;
    std::string token_uri;
  };

  AppConfig config_;
  ServiceAccount service_account_;

  std::mutex token_mutex_;
  std::string access_token_;
  std::int64_t access_token_expires_at_ = 0;

  void load_service_account();
  std::string ensure_access_token();
  std::string request_access_token();
  std::string build_google_assertion() const;
  std::string document_id_for_email(const std::string& email) const;

  std::optional<nlohmann::json> fetch_document(const std::string& document_id);
  bool create_document(const std::string& document_id,
                       const nlohmann::json& document,
                       std::string& error_message);
  UserRecord document_to_user(const nlohmann::json& document) const;
};
