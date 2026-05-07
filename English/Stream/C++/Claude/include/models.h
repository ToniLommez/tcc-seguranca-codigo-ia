#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace soundstream {

struct User {
    std::string id;
    std::string name;
    std::string email;
    std::string password_hash;
    std::string salt;
    std::string type; // "ARTIST" or "USER"
    std::string created_at;
};

struct Song {
    std::string id;
    std::string title;
    std::string genre;
    std::string description;
    std::string file_path;
    std::string artist_id;
    std::string artist_name;
    std::string created_at;
};

struct TokenPayload {
    std::string user_id;
    std::string email;
    std::string type;
    std::string name;
};

inline nlohmann::json user_to_json(const User& u) {
    return {
        {"id", u.id},
        {"name", u.name},
        {"email", u.email},
        {"type", u.type},
        {"created_at", u.created_at}
    };
}

inline nlohmann::json song_to_json(const Song& s) {
    return {
        {"id", s.id},
        {"title", s.title},
        {"genre", s.genre},
        {"description", s.description},
        {"artist_id", s.artist_id},
        {"artist_name", s.artist_name},
        {"created_at", s.created_at}
    };
}

} // namespace soundstream
