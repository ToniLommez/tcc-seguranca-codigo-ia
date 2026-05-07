#pragma once

#include <string>
#include <optional>

namespace jwt_utils {

void init(const std::string& secret);

std::string hash_password(const std::string& password, const std::string& salt);
std::string generate_salt();

std::string create_user_token(const std::string& user_id, const std::string& email);
std::optional<std::pair<std::string, std::string>> verify_user_token(const std::string& token);

std::string create_google_jwt(const std::string& client_email, const std::string& private_key);

}
