#pragma once

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

enum class UserRole {
    Artist,
    User
};

inline std::optional<UserRole> role_from_string(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }

    if (value == "ARTIST") {
        return UserRole::Artist;
    }

    if (value == "USER") {
        return UserRole::User;
    }

    return std::nullopt;
}

inline std::string role_to_string(UserRole role) {
    return role == UserRole::Artist ? "ARTIST" : "USER";
}

struct UserRecord {
    std::string id;
    std::string name;
    std::string email;
    UserRole role{UserRole::User};
    std::string password_hash;
    std::string password_salt;
    std::string created_at;
};

struct JwtClaims {
    std::string subject;
    std::string name;
    std::string email;
    UserRole role{UserRole::User};
    std::int64_t issued_at{};
    std::int64_t expires_at{};
};

struct Song {
    std::string id;
    std::string title;
    std::string genre;
    std::string description;
    std::string artist_id;
    std::string artist_name;
    std::string file_path;
    std::string original_file_name;
    std::string uploaded_at;

    nlohmann::json to_json(bool include_file_path = false) const {
        nlohmann::json json = {
            {"id", id},
            {"title", title},
            {"genre", genre},
            {"description", description},
            {"artistId", artist_id},
            {"artistName", artist_name},
            {"originalFileName", original_file_name},
            {"uploadedAt", uploaded_at}
        };

        if (include_file_path) {
            json["filePath"] = file_path;
        }

        return json;
    }
};
