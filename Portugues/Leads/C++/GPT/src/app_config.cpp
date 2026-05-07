#include "app_config.hpp"

#include <filesystem>
#include <stdexcept>

#include "../external/json.hpp"
#include "utils.hpp"

AppConfig load_config(const std::string& baseDir) {
    const auto configPath = std::filesystem::path(baseDir) / "config.json";
    if (!std::filesystem::exists(configPath)) {
        throw std::runtime_error("Arquivo config.json nao encontrado em " + configPath.string());
    }

    const auto raw = read_text_file(configPath.string());
    const auto parsed = json::parse(raw);

    AppConfig config;
    config.port = parsed.value("port", 8080);
    config.jwtIssuer = parsed.value("jwt_issuer", "lead-manager-cpp");
    config.jwtSecret = parsed.value("jwt_secret", "change-this-secret-in-production-2026");
    config.firebaseServiceAccountPath = parsed.value("firebase_service_account_path", "");
    config.collectionPrefix = parsed.value("collection_prefix", "gpt_5_cpp");
    config.appName = parsed.value("app_name", "Lead Manager C++");

    if (config.firebaseServiceAccountPath.empty()) {
        throw std::runtime_error("Campo firebase_service_account_path nao definido em config.json");
    }

    return config;
}
