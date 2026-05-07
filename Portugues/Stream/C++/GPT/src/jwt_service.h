#pragma once

#include <optional>
#include <string>

#include "models.h"

class JwtService {
public:
  JwtService(std::string secret, int expiration_hours);

  std::string issue_token(const AuthUser& user) const;
  std::optional<AuthUser> verify_token(const std::string& token) const;

private:
  std::string secret_;
  int expiration_hours_ = 12;
};
