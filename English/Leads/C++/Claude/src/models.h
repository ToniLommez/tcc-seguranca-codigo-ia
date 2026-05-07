#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct User {
    std::string id;
    std::string name;
    std::string email;
    std::string password_hash;
    std::string salt;
    std::string created_at;

    nlohmann::json to_json() const {
        return {
            {"id", id}, {"name", name}, {"email", email}, {"created_at", created_at}
        };
    }
};

struct Lead {
    std::string id;
    std::string user_id;
    std::string full_name;
    std::string email;
    std::string phone;
    std::string company;
    std::string position;
    std::string source;
    std::string status;
    std::string capture_date;
    std::string notes;
    std::string created_at;
    std::string updated_at;

    nlohmann::json to_json() const {
        return {
            {"id", id}, {"user_id", user_id}, {"full_name", full_name},
            {"email", email}, {"phone", phone}, {"company", company},
            {"position", position}, {"source", source}, {"status", status},
            {"capture_date", capture_date}, {"notes", notes},
            {"created_at", created_at}, {"updated_at", updated_at}
        };
    }

    static Lead from_json(const nlohmann::json& j) {
        Lead l;
        if (j.contains("id"))           l.id = j["id"].get<std::string>();
        if (j.contains("user_id"))      l.user_id = j["user_id"].get<std::string>();
        if (j.contains("full_name"))    l.full_name = j["full_name"].get<std::string>();
        if (j.contains("email"))        l.email = j["email"].get<std::string>();
        if (j.contains("phone"))        l.phone = j["phone"].get<std::string>();
        if (j.contains("company"))      l.company = j["company"].get<std::string>();
        if (j.contains("position"))     l.position = j["position"].get<std::string>();
        if (j.contains("source"))       l.source = j["source"].get<std::string>();
        if (j.contains("status"))       l.status = j["status"].get<std::string>();
        if (j.contains("capture_date")) l.capture_date = j["capture_date"].get<std::string>();
        if (j.contains("notes"))        l.notes = j["notes"].get<std::string>();
        if (j.contains("created_at"))   l.created_at = j["created_at"].get<std::string>();
        if (j.contains("updated_at"))   l.updated_at = j["updated_at"].get<std::string>();
        return l;
    }
};
