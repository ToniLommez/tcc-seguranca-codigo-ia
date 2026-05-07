#pragma once
#include <string>

namespace Config {
    inline const std::string SERVER_HOST = "0.0.0.0";
    inline const int SERVER_PORT = 8080;

    inline const std::string JWT_SECRET = "music-stream-ultra-secret-key-change-in-prod-2024";
    inline const int JWT_EXPIRY_SECONDS = 86400; // 24h

    inline const std::string FIREBASE_CREDENTIALS_PATH =
        "C:/Users/tonil/Desktop/tcc/lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json";
    inline const std::string FIREBASE_PROJECT_ID = "lead-manager-54595";

    inline const std::string UPLOADS_DIR = "./uploads";
    inline const std::string FRONTEND_DIR = "./frontend";
    inline const std::string DB_PATH     = "./music.db";
}
