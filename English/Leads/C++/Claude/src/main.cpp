#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <iostream>
#include <fstream>

#include "firebase_client.h"
#include "auth_controller.h"
#include "lead_controller.h"
#include "jwt_utils.h"

int main(int argc, char* argv[]) {
    std::string credentials_path = "C:/Users/tonil/Desktop/tcc/"
        "lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json";
    int port = 8080;
    std::string jwt_secret = "lead-manager-jwt-secret-change-in-production";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--credentials" && i + 1 < argc) credentials_path = argv[++i];
        else if (arg == "--secret" && i + 1 < argc) jwt_secret = argv[++i];
    }

    jwt_utils::init(jwt_secret);

    std::cout << "Initializing Firebase client..." << std::endl;
    FirebaseClient firebase(credentials_path);
    if (!firebase.authenticate()) {
        std::cerr << "Failed to authenticate with Firebase" << std::endl;
        return 1;
    }

    AuthController auth(firebase);
    LeadController leads(firebase);

    httplib::Server svr;

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    svr.set_mount_point("/", "./static");

    svr.Post("/api/auth/signup", [&](const httplib::Request& req, httplib::Response& res) {
        auth.signup(req, res);
    });
    svr.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        auth.login(req, res);
    });

    svr.Post("/api/leads", [&](const httplib::Request& req, httplib::Response& res) {
        leads.create(req, res);
    });
    svr.Get("/api/leads/export", [&](const httplib::Request& req, httplib::Response& res) {
        leads.export_csv(req, res);
    });
    svr.Post("/api/leads/import", [&](const httplib::Request& req, httplib::Response& res) {
        leads.import_csv(req, res);
    });
    svr.Get("/api/dashboard", [&](const httplib::Request& req, httplib::Response& res) {
        leads.dashboard(req, res);
    });
    svr.Get(R"(/api/leads/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        leads.get_one(req, res);
    });
    svr.Put(R"(/api/leads/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        leads.update(req, res);
    });
    svr.Delete(R"(/api/leads/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        leads.remove(req, res);
    });
    svr.Get("/api/leads", [&](const httplib::Request& req, httplib::Response& res) {
        leads.get_all(req, res);
    });

    std::cout << "======================================" << std::endl;
    std::cout << "  Lead Manager API Server" << std::endl;
    std::cout << "  http://localhost:" << port << std::endl;
    std::cout << "  Firebase Project: " << firebase.project_id() << std::endl;
    std::cout << "======================================" << std::endl;

    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }

    return 0;
}
