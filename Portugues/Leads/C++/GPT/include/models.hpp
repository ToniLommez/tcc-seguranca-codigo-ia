#pragma once

#include "../external/json.hpp"

using json = nlohmann::json;

struct AuthUser {
    std::string id;
    std::string name;
    std::string email;
    std::string passwordHash;
    std::string createdAt;
    std::string updatedAt;
};

struct LeadRecord {
    std::string id;
    std::string userId;
    std::string fullName;
    std::string email;
    std::string phone;
    std::string company;
    std::string jobTitle;
    std::string source;
    std::string status;
    std::string captureDate;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
    json interactions = json::array();
};

