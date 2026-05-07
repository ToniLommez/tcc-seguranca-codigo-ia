#pragma once

#include <string>

struct AppConfig {
    int port = 8080;
    std::string jwtIssuer = "lead-manager-cpp";
    std::string jwtSecret = "change-this-secret-in-production-2026";
    std::string firebaseServiceAccountPath;
    std::string collectionPrefix = "gpt_5_cpp";
    std::string appName = "Lead Manager C++";
};

AppConfig load_config(const std::string& baseDir);

