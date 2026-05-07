#include "app_config.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace {
std::filesystem::path resolve_path(const std::filesystem::path& base_dir,
                                   const std::filesystem::path& path_value) {
  if (path_value.is_absolute()) {
    return path_value;
  }

  return std::filesystem::weakly_canonical(base_dir / path_value);
}
} // namespace

AppConfig AppConfig::load_from_file(const std::filesystem::path& config_path) {
  std::ifstream stream(config_path);
  if (!stream) {
    throw std::runtime_error("Nao foi possivel abrir o arquivo de configuracao: " +
                             config_path.string());
  }

  nlohmann::json root = nlohmann::json::parse(stream);
  const auto base_dir = std::filesystem::absolute(config_path).parent_path();

  AppConfig config;
  config.host = root.at("server").value("host", "0.0.0.0");
  config.port = root.at("server").value("port", 8080);
  config.max_upload_mb = root.at("server").value("maxUploadMb", 30);

  config.frontend_dir = resolve_path(base_dir, root.at("paths").at("frontendDir").get<std::string>());
  config.uploads_dir = resolve_path(base_dir, root.at("paths").at("uploadsDir").get<std::string>());
  config.songs_catalog_path = resolve_path(base_dir, root.at("paths").at("songsCatalogPath").get<std::string>());

  config.jwt_secret = root.at("auth").at("jwtSecret").get<std::string>();
  config.jwt_expiration_hours = root.at("auth").value("jwtExpirationHours", 12);

  config.firebase_service_account_path =
    resolve_path(base_dir, root.at("firebase").at("serviceAccountPath").get<std::string>());
  config.firebase_collection = root.at("firebase").value("collection", "users");

  return config;
}

void AppConfig::prepare_runtime() const {
  std::filesystem::create_directories(frontend_dir);
  std::filesystem::create_directories(uploads_dir);
  std::filesystem::create_directories(songs_catalog_path.parent_path());
}
