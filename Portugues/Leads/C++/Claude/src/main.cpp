#include <crow.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

#include "config.h"
#include "firebase/firebase_client.h"
#include "auth/auth_routes.h"
#include "leads/leads_routes.h"

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ---- Static file helpers -----------------------------------------------
static std::string getMimeType(const std::string& path) {
    auto ext = path.substr(path.rfind('.') + 1);
    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "css")  return "text/css";
    if (ext == "js")   return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "ico")  return "image/x-icon";
    return "application/octet-stream";
}

static crow::response serveFile(const std::string& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) return crow::response(404);
    std::ostringstream ss;
    ss << f.rdbuf();
    crow::response res(200, ss.str());
    res.add_header("Content-Type", getMimeType(filePath));
    return res;
}

// ---- CORS middleware ---------------------------------------------------
struct CorsMiddleware {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context&) {
        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods",
                       "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers",
                       "Content-Type, Authorization");
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 204;
            res.end();
        }
    }
    void after_handle(crow::request&, crow::response&, context&) {}
};

// ---- Main --------------------------------------------------------------
int main() {
    std::cout << "============================================\n";
    std::cout << "   Lead Manager - C++ REST Service\n";
    std::cout << "   Modelo: claude-sonnet-4-6\n";
    std::cout << "============================================\n\n";

    // Initialise Firebase client
    std::shared_ptr<FirebaseClient> firebase;
    try {
        firebase = std::make_shared<FirebaseClient>(FIREBASE_CREDENTIALS_PATH);
        std::cout << "[OK] Firebase conectado (projeto: " << FIREBASE_PROJECT_ID << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "[ERRO] Firebase: " << e.what() << "\n";
        return 1;
    }

    crow::SimpleApp app;

    // ---- API Routes ----------------------------------------------------
    setupAuthRoutes(app, firebase);
    setupLeadsRoutes(app, firebase);

    // ---- Health check --------------------------------------------------
    CROW_ROUTE(app, "/api/health")
    ([]() -> crow::response {
        crow::response res(200,
            R"({"status":"ok","service":"lead-manager","model":"claude-sonnet-4-6"})");
        res.add_header("Content-Type", "application/json");
        return res;
    });

    // ---- Static frontend files -----------------------------------------
    // Root → index.html (login page)
    CROW_ROUTE(app, "/")
    ([]() -> crow::response {
        return serveFile(std::string(FRONTEND_DIR) + "/index.html");
    });

    CROW_ROUTE(app, "/dashboard")
    ([]() -> crow::response {
        return serveFile(std::string(FRONTEND_DIR) + "/dashboard.html");
    });

    CROW_ROUTE(app, "/leads")
    ([]() -> crow::response {
        return serveFile(std::string(FRONTEND_DIR) + "/leads.html");
    });

    CROW_ROUTE(app, "/leads/<string>")
    ([](const std::string&) -> crow::response {
        return serveFile(std::string(FRONTEND_DIR) + "/lead-detail.html");
    });

    // Generic static file serving (css, js, images, etc.)
    CROW_ROUTE(app, "/css/<path>")
    ([](const std::string& path) -> crow::response {
        return serveFile(std::string(FRONTEND_DIR) + "/css/" + path);
    });

    CROW_ROUTE(app, "/js/<path>")
    ([](const std::string& path) -> crow::response {
        return serveFile(std::string(FRONTEND_DIR) + "/js/" + path);
    });

    // ---- Start server --------------------------------------------------
    std::cout << "[INFO] Servidor iniciando em http://localhost:"
              << SERVER_PORT << "\n";
    std::cout << "[INFO] Colecoes: " << COLLECTION_USERS
              << " | " << COLLECTION_LEADS << "\n\n";

    app.bindaddr(SERVER_HOST)
       .port(SERVER_PORT)
       .multithreaded()
       .run();

    return 0;
}
