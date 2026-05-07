#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app_config.hpp"
#include "models.hpp"
#include "security.hpp"

class FirebaseClient {
public:
    explicit FirebaseClient(const AppConfig& config);

    bool ping(std::string& errorMessage);

    bool create_user(const AuthUser& user, std::string& errorMessage);
    std::optional<AuthUser> get_user_by_email(const std::string& email, std::string& errorMessage);

    bool create_lead(const LeadRecord& lead, std::string& errorMessage);
    std::optional<LeadRecord> get_lead(const std::string& leadId, std::string& errorMessage);
    std::vector<LeadRecord> list_leads_for_user(const std::string& userId, std::string& errorMessage);
    bool update_lead(const LeadRecord& lead, std::string& errorMessage);
    bool delete_lead(const std::string& leadId, std::string& errorMessage);

    std::string users_collection() const;
    std::string leads_collection() const;

private:
    std::string get_access_token(std::string& errorMessage);
    std::string firestore_base_url() const;
    json to_firestore_document(const json& source) const;
    json from_firestore_document(const json& document) const;
    json to_firestore_value(const json& value) const;
    json from_firestore_value(const json& value) const;
    std::optional<json> fetch_document(const std::string& collection, const std::string& documentId, std::string& errorMessage);
    bool write_document(const std::string& collection, const std::string& documentId, const json& body, bool createOnly, std::string& errorMessage);

    AppConfig config_;
    ServiceAccountCredentials credentials_;
    std::string usersCollection_;
    std::string leadsCollection_;
    std::string accessToken_;
    long long accessTokenExpiresAt_ = 0;
};

