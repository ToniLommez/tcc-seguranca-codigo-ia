#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lead_manager/app_config.hpp"
#include "lead_manager/http_client.hpp"
#include "lead_manager/models.hpp"

namespace lead_manager {

class FirestoreClient {
 public:
  explicit FirestoreClient(AppConfig config);

  std::optional<User> getUserByEmail(const std::string& email);
  std::optional<User> getUserById(const std::string& userId);
  bool upsertUser(const User& user);

  std::vector<Lead> listLeadsForUser(const std::string& userId);
  std::optional<Lead> getLead(const std::string& userId, const std::string& leadId);
  bool upsertLead(const Lead& lead);
  bool deleteLead(const std::string& userId, const std::string& leadId);

 private:
  using json = nlohmann::json;

  std::string getAccessToken();
  json runQuery(const json& structuredQuery);
  std::optional<json> getDocument(const std::string& collection, const std::string& documentId);
  bool setDocument(const std::string& collection, const std::string& documentId, const json& fields);
  bool deleteDocument(const std::string& collection, const std::string& documentId);
  std::string documentsBaseUrl() const;
  std::map<std::string, std::string> authHeaders();

  AppConfig config_;
  HttpClient http_;
  mutable std::string accessToken_;
  mutable std::time_t accessTokenExpiry_ = 0;
  std::string clientEmail_;
  std::string privateKey_;
};

}  // namespace lead_manager
