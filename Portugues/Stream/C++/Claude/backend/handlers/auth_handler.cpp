#include "auth_handler.h"
#include "auth/jwt_helper.h"
#include "config.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>

// --------------------------------------------------------------------------
// Password hashing (PBKDF2-SHA256)
// --------------------------------------------------------------------------
static std::string bytesToHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return oss.str();
}

static std::vector<unsigned char> hexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned int b;
        std::istringstream ss(hex.substr(i, 2));
        ss >> std::hex >> b;
        bytes.push_back((unsigned char)b);
    }
    return bytes;
}

static std::string generateSalt() {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    return bytesToHex(salt, sizeof(salt));
}

static std::string hashPassword(const std::string& password, const std::string& saltHex) {
    auto saltBytes = hexToBytes(saltHex);
    unsigned char hash[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                       saltBytes.data(), (int)saltBytes.size(),
                       10000, EVP_sha256(),
                       sizeof(hash), hash);
    return bytesToHex(hash, sizeof(hash));
}

static bool verifyPassword(const std::string& password,
                            const std::string& saltHex,
                            const std::string& expectedHash) {
    return hashPassword(password, saltHex) == expectedHash;
}

static void json_response(httplib::Response& res, int status, const nlohmann::json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

// --------------------------------------------------------------------------
AuthHandler::AuthHandler(FirebaseService& firebase) : m_firebase(firebase) {}

void AuthHandler::registerRoutes(httplib::Server& svr) {
    svr.Post("/api/auth/register", [this](const httplib::Request& req, httplib::Response& res) {
        handleRegister(req, res);
    });
    svr.Post("/api/auth/login", [this](const httplib::Request& req, httplib::Response& res) {
        handleLogin(req, res);
    });
}

void AuthHandler::handleRegister(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); }
    catch (...) { json_response(res, 400, {{"error", "JSON inválido"}}); return; }

    std::string name  = body.value("name", "");
    std::string email = body.value("email", "");
    std::string pass  = body.value("password", "");
    std::string type  = body.value("type", "");

    if (name.empty() || email.empty() || pass.empty()) {
        json_response(res, 400, {{"error", "nome, email e senha são obrigatórios"}}); return;
    }
    if (type != "ARTISTA" && type != "USUARIO") {
        json_response(res, 400, {{"error", "type deve ser ARTISTA ou USUARIO"}}); return;
    }

    // Check if email already exists
    if (m_firebase.getUserByEmail(email)) {
        json_response(res, 409, {{"error", "Email já cadastrado"}}); return;
    }

    std::string salt = generateSalt();
    std::string hash = hashPassword(pass, salt);

    UserDocument user;
    user.name         = name;
    user.email        = email;
    user.passwordHash = hash;
    user.passwordSalt = salt;
    user.type         = type;

    if (!m_firebase.createUser(user)) {
        json_response(res, 500, {{"error", "Erro ao criar usuário"}}); return;
    }

    json_response(res, 201, {{"message", "Usuário criado com sucesso"}});
}

void AuthHandler::handleLogin(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); }
    catch (...) { json_response(res, 400, {{"error", "JSON inválido"}}); return; }

    std::string email = body.value("email", "");
    std::string pass  = body.value("password", "");

    if (email.empty() || pass.empty()) {
        json_response(res, 400, {{"error", "email e senha são obrigatórios"}}); return;
    }

    auto userOpt = m_firebase.getUserByEmail(email);
    if (!userOpt) {
        json_response(res, 401, {{"error", "Credenciais inválidas"}}); return;
    }

    if (!verifyPassword(pass, userOpt->passwordSalt, userOpt->passwordHash)) {
        json_response(res, 401, {{"error", "Credenciais inválidas"}}); return;
    }

    std::string token = JwtHelper::createToken(userOpt->id, userOpt->email,
                                               userOpt->type, Config::JWT_SECRET);

    json_response(res, 200, {
        {"token",    token},
        {"type",     userOpt->type},
        {"name",     userOpt->name},
        {"email",    userOpt->email}
    });
}
