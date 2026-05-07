#pragma once

#include <string>

namespace lead_manager {

struct AppConfig {
  std::string host;
  int port;
  std::string jwtSecret;
  std::string firebaseCredentialsPath;
  std::string frontendPath;
  std::string firestoreProjectId;
  std::string collectionPrefix;
  std::string usersCollection;
  std::string leadsCollection;

  static AppConfig load();
};

}  // namespace lead_manager

