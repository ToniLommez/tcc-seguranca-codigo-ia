#pragma once

#include <string>

class PasswordService {
  public:
    static bool hash_password(
        const std::string& password,
        std::string& encoded_salt,
        std::string& encoded_hash,
        std::string& error
    );

    static bool verify_password(
        const std::string& password,
        const std::string& encoded_salt,
        const std::string& encoded_hash,
        std::string& error
    );
};

