#pragma once

#include <filesystem>
#include <string>

struct AppConfig {
  std::string host = "0.0.0.0";
  int port = 8080;
  std::size_t max_upload_mb = 30;

  std::filesystem::path frontend_dir;
  std::filesystem::path uploads_dir;
  std::filesystem::path songs_catalog_path;

  std::string jwt_secret;
  int jwt_expiration_hours = 12;

  std::filesystem::path firebase_service_account_path;
  std::string firebase_collection = "users";

  static AppConfig load_from_file(const std::filesystem::path& config_path);
  void prepare_runtime() const;
};
