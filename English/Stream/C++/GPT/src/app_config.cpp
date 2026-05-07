#include "app_config.hpp"

#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

namespace {

std::string env_or_default(const char* key, const std::string& fallback) {
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, key) == 0 && buffer != nullptr) {
        std::string value(buffer);
        std::free(buffer);
        if (!value.empty()) {
            return value;
        }
    } else if (buffer != nullptr) {
        std::free(buffer);
    }

    return fallback;
}

int env_int_or_default(const char* key, int fallback) {
    const auto value = env_or_default(key, "");
    if (value.empty()) {
        return fallback;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

AppConfig AppConfig::load(const std::filesystem::path& config_path) {
    AppConfig config;

    if (std::ifstream stream(config_path); stream) {
        nlohmann::json json;
        stream >> json;

        config.port = json.value("port", config.port);
        config.jwt_secret = json.value("jwt_secret", config.jwt_secret);
        config.frontend_dir = json.value("frontend_dir", config.frontend_dir.string());
        config.uploads_dir = json.value("uploads_dir", config.uploads_dir.string());
        config.songs_data_path = json.value("songs_data_path", config.songs_data_path.string());
        config.firebase_service_account_path = json.value(
            "firebase_service_account_path",
            config.firebase_service_account_path.string()
        );
        config.firebase_project_id = json.value("firebase_project_id", config.firebase_project_id);
    }

    config.port = env_int_or_default("STREAM_APP_PORT", config.port);
    config.jwt_secret = env_or_default("STREAM_APP_JWT_SECRET", config.jwt_secret);
    config.firebase_project_id = env_or_default("STREAM_APP_FIREBASE_PROJECT_ID", config.firebase_project_id);
    config.firebase_service_account_path =
        env_or_default("STREAM_APP_FIREBASE_SERVICE_ACCOUNT", config.firebase_service_account_path.string());

    return config;
}
