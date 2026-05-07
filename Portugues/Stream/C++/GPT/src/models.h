#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

enum class UserType {
  Artist,
  User
};

inline std::string user_type_to_string(UserType type) {
  return type == UserType::Artist ? "ARTISTA" : "USUARIO";
}

inline std::optional<UserType> user_type_from_string(std::string_view raw) {
  std::string value(raw);
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });

  if (value == "ARTISTA" || value == "ARTIST") {
    return UserType::Artist;
  }

  if (value == "USUARIO" || value == "USER" || value == "USUÁRIO") {
    return UserType::User;
  }

  return std::nullopt;
}

struct UserRecord {
  std::string name;
  std::string email;
  std::string password_hash;
  UserType type = UserType::User;
  std::string created_at;
};

struct AuthUser {
  std::string name;
  std::string email;
  UserType type = UserType::User;
};

struct SongRecord {
  std::string id;
  std::string title;
  std::string genre;
  std::string description;
  std::string artist_email;
  std::string artist_name;
  std::string file_name;
  std::string file_path;
  std::string created_at;
};

inline nlohmann::json to_public_json(const UserRecord& user) {
  return {
    {"name", user.name},
    {"email", user.email},
    {"type", user_type_to_string(user.type)},
    {"createdAt", user.created_at}
  };
}

inline nlohmann::json to_public_json(const AuthUser& user) {
  return {
    {"name", user.name},
    {"email", user.email},
    {"type", user_type_to_string(user.type)}
  };
}

inline nlohmann::json to_json(const SongRecord& song) {
  return {
    {"id", song.id},
    {"title", song.title},
    {"genre", song.genre},
    {"description", song.description},
    {"artistEmail", song.artist_email},
    {"artistName", song.artist_name},
    {"fileName", song.file_name},
    {"filePath", song.file_path},
    {"createdAt", song.created_at}
  };
}
