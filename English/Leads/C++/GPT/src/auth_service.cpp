#include "lead_manager/auth_service.hpp"

#include <ctime>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "lead_manager/utils.hpp"

namespace lead_manager {

namespace {

std::vector<std::string> splitToken(const std::string& token, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream stream(token);
  std::string item;
  while (std::getline(stream, item, delimiter)) {
    parts.push_back(item);
  }
  return parts;
}

}  // namespace

AuthService::AuthService(AppConfig config, FirestoreClient& firestore)
    : config_(std::move(config)), firestore_(firestore) {}

bool AuthService::registerUser(const std::string& name,
                               const std::string& email,
                               const std::string& password,
                               User& createdUser,
                               std::string& error) {
  const std::string normalizedName = util::trim(name);
  const std::string normalizedEmail = util::trim(email);
  const std::string normalizedEmailLower = util::toLower(normalizedEmail);

  if (normalizedName.empty()) {
    error = "Name is required.";
    return false;
  }
  if (normalizedEmail.empty() || normalizedEmail.find('@') == std::string::npos) {
    error = "A valid email is required.";
    return false;
  }
  if (password.size() < 8) {
    error = "Password must be at least 8 characters.";
    return false;
  }

  if (firestore_.getUserByEmail(normalizedEmailLower).has_value()) {
    error = "A user with this email already exists.";
    return false;
  }

  User user;
  user.id = util::generateId();
  user.name = normalizedName;
  user.email = normalizedEmail;
  user.emailLower = normalizedEmailLower;
  user.passwordSalt = util::randomHex(16);
  user.passwordHash = util::pbkdf2Hash(password, user.passwordSalt);
  user.createdAt = util::nowUtcIso();

  firestore_.upsertUser(user);
  createdUser = user;
  return true;
}

bool AuthService::login(const std::string& email,
                        const std::string& password,
                        std::string& token,
                        User& user,
                        std::string& error) {
  const auto storedUser = firestore_.getUserByEmail(util::trim(email));
  if (!storedUser.has_value()) {
    error = "Invalid email or password.";
    return false;
  }

  if (util::pbkdf2Hash(password, storedUser->passwordSalt) != storedUser->passwordHash) {
    error = "Invalid email or password.";
    return false;
  }

  user = storedUser.value();
  token = issueToken(user);
  return true;
}

std::string AuthService::issueToken(const User& user) const {
  const std::time_t now = std::time(nullptr);
  const nlohmann::json header = {{"alg", "HS256"}, {"typ", "JWT"}};
  const nlohmann::json payload = {
      {"sub", user.id},
      {"email", user.email},
      {"name", user.name},
      {"iat", static_cast<long long>(now)},
      {"exp", static_cast<long long>(now + 60 * 60 * 24)},
  };

  const std::string unsignedToken =
      util::base64UrlEncode(header.dump()) + "." + util::base64UrlEncode(payload.dump());
  const std::string signature = util::hmacSha256Base64Url(config_.jwtSecret, unsignedToken);
  return unsignedToken + "." + signature;
}

std::optional<AuthenticatedUser> AuthService::authenticateHeader(const std::string& authorizationHeader) const {
  const std::string prefix = "Bearer ";
  if (authorizationHeader.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }

  const std::string token = authorizationHeader.substr(prefix.size());
  const auto parts = splitToken(token, '.');
  if (parts.size() != 3) {
    return std::nullopt;
  }

  const std::string unsignedToken = parts[0] + "." + parts[1];
  const std::string expectedSignature = util::hmacSha256Base64Url(config_.jwtSecret, unsignedToken);
  if (expectedSignature != parts[2]) {
    return std::nullopt;
  }

  const auto payload = nlohmann::json::parse(util::base64UrlDecode(parts[1]), nullptr, false);
  if (payload.is_discarded()) {
    return std::nullopt;
  }

  const auto now = static_cast<long long>(std::time(nullptr));
  if (payload.value("exp", 0LL) < now) {
    return std::nullopt;
  }

  return AuthenticatedUser{
      .id = payload.value("sub", ""),
      .email = payload.value("email", ""),
      .name = payload.value("name", ""),
  };
}

}  // namespace lead_manager

