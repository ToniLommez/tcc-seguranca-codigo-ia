#include "lead_manager/app_config.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "lead_manager/utils.hpp"

namespace lead_manager {

namespace {

std::string envOrDefault(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string(value).empty()) {
    return fallback;
  }
  return value;
}

int envOrDefaultInt(const char* name, int fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string(value).empty()) {
    return fallback;
  }
  return std::stoi(value);
}

}  // namespace

AppConfig AppConfig::load() {
  AppConfig config;
  config.host = envOrDefault("LEAD_MANAGER_HOST", "0.0.0.0");
  config.port = envOrDefaultInt("LEAD_MANAGER_PORT", 8080);
  config.jwtSecret = envOrDefault("LEAD_MANAGER_JWT_SECRET", "change-this-jwt-secret");
  config.firebaseCredentialsPath =
      envOrDefault("LEAD_MANAGER_FIREBASE_CREDENTIALS",
                   R"(C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json)");
  config.frontendPath = envOrDefault("LEAD_MANAGER_FRONTEND_PATH", "frontend");
  config.collectionPrefix = envOrDefault("LEAD_MANAGER_COLLECTION_PREFIX", "gpt5_cpp");

  const auto credentialsJson =
      nlohmann::json::parse(util::readFile(config.firebaseCredentialsPath));
  config.firestoreProjectId = credentialsJson.value("project_id", "");
  if (config.firestoreProjectId.empty()) {
    throw std::runtime_error("Firebase credentials file does not contain a project_id.");
  }

  config.usersCollection = config.collectionPrefix + "_users";
  config.leadsCollection = config.collectionPrefix + "_leads";

  return config;
}

}  // namespace lead_manager

