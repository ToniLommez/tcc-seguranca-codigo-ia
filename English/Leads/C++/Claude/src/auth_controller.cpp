#include "auth_controller.h"
#include "jwt_utils.h"
#include <iostream>

using json = nlohmann::json;

AuthController::AuthController(FirebaseClient& firebase) : firebase_(firebase) {}

std::string AuthController::extract_user_id(const httplib::Request& req) {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) return "";

    auto& header = it->second;
    if (header.substr(0, 7) != "Bearer ") return "";

    auto token = header.substr(7);
    auto result = jwt_utils::verify_user_token(token);
    if (!result) return "";
    return result->first;
}

void AuthController::signup(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);

        std::string name = body.value("name", "");
        std::string email = body.value("email", "");
        std::string password = body.value("password", "");

        if (name.empty() || email.empty() || password.empty()) {
            res.status = 400;
            res.set_content(json({{"error", "Name, email, and password are required"}}).dump(),
                           "application/json");
            return;
        }

        if (password.length() < 6) {
            res.status = 400;
            res.set_content(json({{"error", "Password must be at least 6 characters"}}).dump(),
                           "application/json");
            return;
        }

        json query = {
            {"from", {{{"collectionId", COLLECTION}}}},
            {"where", {
                {"fieldFilter", {
                    {"field", {{"fieldPath", "email"}}},
                    {"op", "EQUAL"},
                    {"value", {{"stringValue", email}}}
                }}
            }},
            {"limit", 1}
        };
        auto existing = firebase_.run_query(COLLECTION, query);
        if (!existing.documents.empty()) {
            res.status = 409;
            res.set_content(json({{"error", "Email already registered"}}).dump(),
                           "application/json");
            return;
        }

        std::string salt = jwt_utils::generate_salt();
        std::string hash = jwt_utils::hash_password(password, salt);

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif
        char time_str[64];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);

        json user_data = {
            {"name", name},
            {"email", email},
            {"password_hash", hash},
            {"salt", salt},
            {"created_at", std::string(time_str)}
        };

        auto doc_id = firebase_.create_document(COLLECTION, user_data);
        if (!doc_id) {
            res.status = 500;
            res.set_content(json({{"error", "Failed to create user"}}).dump(),
                           "application/json");
            return;
        }

        auto token = jwt_utils::create_user_token(*doc_id, email);

        json response = {
            {"token", token},
            {"user", {
                {"id", *doc_id},
                {"name", name},
                {"email", email}
            }}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", std::string("Invalid request: ") + e.what()}}).dump(),
                       "application/json");
    }
}

void AuthController::login(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);

        std::string email = body.value("email", "");
        std::string password = body.value("password", "");

        if (email.empty() || password.empty()) {
            res.status = 400;
            res.set_content(json({{"error", "Email and password are required"}}).dump(),
                           "application/json");
            return;
        }

        json query = {
            {"from", {{{"collectionId", COLLECTION}}}},
            {"where", {
                {"fieldFilter", {
                    {"field", {{"fieldPath", "email"}}},
                    {"op", "EQUAL"},
                    {"value", {{"stringValue", email}}}
                }}
            }},
            {"limit", 1}
        };

        auto result = firebase_.run_query(COLLECTION, query);
        if (result.documents.empty()) {
            res.status = 401;
            res.set_content(json({{"error", "Invalid email or password"}}).dump(),
                           "application/json");
            return;
        }

        auto& user = result.documents[0];
        std::string stored_hash = user["password_hash"].get<std::string>();
        std::string salt = user["salt"].get<std::string>();
        std::string computed_hash = jwt_utils::hash_password(password, salt);

        if (computed_hash != stored_hash) {
            res.status = 401;
            res.set_content(json({{"error", "Invalid email or password"}}).dump(),
                           "application/json");
            return;
        }

        std::string user_id = user["id"].get<std::string>();
        auto token = jwt_utils::create_user_token(user_id, email);

        json response = {
            {"token", token},
            {"user", {
                {"id", user_id},
                {"name", user["name"]},
                {"email", email}
            }}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", std::string("Invalid request: ") + e.what()}}).dump(),
                       "application/json");
    }
}
