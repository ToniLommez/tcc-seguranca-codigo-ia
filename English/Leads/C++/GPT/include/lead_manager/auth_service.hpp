#pragma once

#include <optional>
#include <string>

#include "lead_manager/app_config.hpp"
#include "lead_manager/firestore_client.hpp"
#include "lead_manager/models.hpp"

namespace lead_manager {

class AuthService {
 public:
  AuthService(AppConfig config, FirestoreClient& firestore);

  bool registerUser(const std::string& name,
                    const std::string& email,
                    const std::string& password,
                    User& createdUser,
                    std::string& error);

  bool login(const std::string& email,
             const std::string& password,
             std::string& token,
             User& user,
             std::string& error);

  std::optional<AuthenticatedUser> authenticateHeader(const std::string& authorizationHeader) const;

 private:
  std::string issueToken(const User& user) const;

  AppConfig config_;
  FirestoreClient& firestore_;
};

}  // namespace lead_manager

