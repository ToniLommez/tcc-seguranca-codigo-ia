#include "auth_routes.h"
#include "../config.h"
#include "../utils/hash_utils.h"
#include "../utils/jwt_utils.h"
#include <nlohmann/json.hpp>
#include <crow.h>

using json = nlohmann::json;

// ---- Helper: extract & verify Bearer token from request ----------------
static std::tuple<bool, std::string, std::string>
requireAuth(const crow::request& req) {
    auto auth = req.get_header_value("Authorization");
    if (auth.size() < 8 || auth.substr(0, 7) != "Bearer ")
        return {false, "", ""};
    return JwtUtils::verifyToken(auth.substr(7), JWT_SECRET);
}

static crow::response jsonResp(int code, const json& body) {
    crow::response res(code, body.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

// ---- Route setup -------------------------------------------------------
void setupAuthRoutes(crow::SimpleApp& app,
                     std::shared_ptr<FirebaseClient> firebase) {

    // POST /api/auth/register
    CROW_ROUTE(app, "/api/auth/register")
    .methods("POST"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            return jsonResp(400, {{"error", "Invalid JSON"}});
        }

        std::string name  = body.value("name",  "");
        std::string email = body.value("email", "");
        std::string pass  = body.value("password", "");

        if (name.empty() || email.empty() || pass.empty())
            return jsonResp(400, {{"error", "name, email e password sao obrigatorios"}});
        if (pass.size() < 6)
            return jsonResp(400, {{"error", "Senha deve ter pelo menos 6 caracteres"}});

        try {
            // Check if e-mail is already registered
            json query = {
                {"from",  json::array({{{"collectionId", COLLECTION_USERS}}})},
                {"where", {
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "email"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", email}}}
                    }}
                }},
                {"limit", 1}
            };
            auto existing = firebase->runQuery(query);
            if (!existing.empty())
                return jsonResp(409, {{"error", "E-mail ja cadastrado"}});

            std::string id           = HashUtils::generateUUID();
            std::string passwordHash = HashUtils::hashPasswordNew(pass);

            using namespace std::chrono;
            auto now = duration_cast<seconds>(
                system_clock::now().time_since_epoch()).count();

            json userData = {
                {"id",           id},
                {"name",         name},
                {"email",        email},
                {"password_hash",passwordHash},
                {"created_at",   std::to_string(now)}
            };
            firebase->createDocument(COLLECTION_USERS, userData);

            std::string token = JwtUtils::createToken(id, email, JWT_SECRET);
            return jsonResp(201, {
                {"message", "Usuario criado com sucesso"},
                {"token",   token},
                {"user",    {{"id", id}, {"name", name}, {"email", email}}}
            });
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // POST /api/auth/login
    CROW_ROUTE(app, "/api/auth/login")
    .methods("POST"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            return jsonResp(400, {{"error", "Invalid JSON"}});
        }

        std::string email = body.value("email",    "");
        std::string pass  = body.value("password", "");

        if (email.empty() || pass.empty())
            return jsonResp(400, {{"error", "email e password sao obrigatorios"}});

        try {
            json query = {
                {"from",  json::array({{{"collectionId", COLLECTION_USERS}}})},
                {"where", {
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "email"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", email}}}
                    }}
                }},
                {"limit", 1}
            };
            auto users = firebase->runQuery(query);
            if (users.empty())
                return jsonResp(401, {{"error", "Credenciais invalidas"}});

            auto& user = users[0];
            std::string storedHash = user.value("password_hash", "");
            if (!HashUtils::verifyPassword(pass, storedHash))
                return jsonResp(401, {{"error", "Credenciais invalidas"}});

            std::string id    = user.value("id",   "");
            std::string name  = user.value("name", "");
            std::string token = JwtUtils::createToken(id, email, JWT_SECRET);

            return jsonResp(200, {
                {"token", token},
                {"user",  {{"id", id}, {"name", name}, {"email", email}}}
            });
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // GET /api/auth/me  (protected)
    CROW_ROUTE(app, "/api/auth/me")
    .methods("GET"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        try {
            json query = {
                {"from",  json::array({{{"collectionId", COLLECTION_USERS}}})},
                {"where", {
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "id"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", userId}}}
                    }}
                }},
                {"limit", 1}
            };
            auto users = firebase->runQuery(query);
            if (users.empty())
                return jsonResp(404, {{"error", "Usuario nao encontrado"}});

            auto& user = users[0];
            return jsonResp(200, {
                {"id",    user.value("id",    "")},
                {"name",  user.value("name",  "")},
                {"email", user.value("email", "")}
            });
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });
}
