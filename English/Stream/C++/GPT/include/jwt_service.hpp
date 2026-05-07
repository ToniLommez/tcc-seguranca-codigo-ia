#pragma once

#include <optional>
#include <string>

#include "models.hpp"

class JwtService {
  public:
    explicit JwtService(std::string secret);

    std::string create_token(const UserRecord& user, int expires_in_minutes = 180) const;
    std::optional<JwtClaims> verify_token(const std::string& token) const;

  private:
    std::string secret_;
};

