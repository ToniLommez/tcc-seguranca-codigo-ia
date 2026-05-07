#pragma once

#include <optional>
#include <string>

#include "app_config.hpp"
#include "models.hpp"

struct JwtPayload {
    std::string userId;
    std::string email;
    std::string name;
    long long exp = 0;
};

struct ServiceAccountCredentials {
    std::string projectId;
    std::string privateKeyId;
    std::string privateKeyPem;
    std::string clientEmail;
    std::string tokenUri;
};

ServiceAccountCredentials load_service_account(const std::string& path);
std::string hash_password(const std::string& password);
bool verify_password(const std::string& password, const std::string& encodedHash);
std::string create_app_jwt(const AppConfig& config, const AuthUser& user);
std::optional<JwtPayload> verify_app_jwt(const AppConfig& config, const std::string& token);
std::string sign_service_account_jwt(
    const ServiceAccountCredentials& credentials,
    const std::string& audience,
    const json& claims
);

