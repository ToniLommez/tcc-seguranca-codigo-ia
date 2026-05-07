#define CPPHTTPLIB_OPENSSL_SUPPORT
// MinGW ws2tcpip.h is missing GetAddrInfoExCancel — declare it manually
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef GetAddrInfoExCancel
extern "C" INT WSAAPI GetAddrInfoExCancel(LPHANDLE lpHandle);
#endif
#endif
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <filesystem>

#include "config.h"
#include "firebase/firebase_service.h"
#include "db/local_db.h"
#include "handlers/auth_handler.h"
#include "handlers/music_handler.h"
#include "handlers/stream_handler.h"

namespace fs = std::filesystem;

static void addCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

static std::string guessMime(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css"))  return "text/css";
    if (path.ends_with(".js"))   return "application/javascript";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".svg"))  return "image/svg+xml";
    if (path.ends_with(".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

int main() {
    std::cout << "=== Music Streaming Server ===\n";

    // Ensure uploads dir exists
    fs::create_directories(Config::UPLOADS_DIR);

    // Initialize SQLite
    LocalDb db(Config::DB_PATH);
    if (!db.initialize()) {
        std::cerr << "Failed to initialize database\n";
        return 1;
    }
    std::cout << "[OK] Database initialized: " << Config::DB_PATH << "\n";

    // Initialize Firebase
    FirebaseService firebase(Config::FIREBASE_CREDENTIALS_PATH);
    std::cout << "[OK] Firebase initialized\n";

    // HTTP Server
    httplib::Server svr;

    // CORS preflight
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        addCorsHeaders(res);
        res.status = 204;
    });

    // Add CORS to all responses
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        addCorsHeaders(res);
    });

    // Register API routes
    AuthHandler   authHandler(firebase);
    MusicHandler  musicHandler(db);
    StreamHandler streamHandler(db);

    authHandler.registerRoutes(svr);
    musicHandler.registerRoutes(svr);
    streamHandler.registerRoutes(svr);

    // Serve frontend static files
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream f(Config::FRONTEND_DIR + "/index.html");
        if (!f) { res.status = 404; return; }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        res.set_content(content, "text/html");
    });

    svr.Get(R"(/([^/].*\.(html|css|js|png|ico|svg)))", [](const httplib::Request& req, httplib::Response& res) {
        std::string subpath = req.matches[1];
        std::string fullPath = Config::FRONTEND_DIR + "/" + subpath;
        std::ifstream f(fullPath, std::ios::binary);
        if (!f) { res.status = 404; return; }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        res.set_content(content, guessMime(fullPath));
    });

    // Deep paths (artist/, user/, assets/...)
    svr.Get(R"(/((?:[^/]+/)+[^/]+\.(html|css|js|png|ico|svg)))", [](const httplib::Request& req, httplib::Response& res) {
        std::string subpath = req.matches[1];
        std::string fullPath = Config::FRONTEND_DIR + "/" + subpath;
        std::ifstream f(fullPath, std::ios::binary);
        if (!f) { res.status = 404; return; }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        res.set_content(content, guessMime(fullPath));
    });

    std::cout << "[OK] Servidor iniciado em http://localhost:" << Config::SERVER_PORT << "\n";
    std::cout << "     Acesse http://localhost:" << Config::SERVER_PORT << " no navegador\n";
    std::cout << "     Pressione Ctrl+C para parar\n\n";

    svr.listen(Config::SERVER_HOST.c_str(), Config::SERVER_PORT);
    return 0;
}
