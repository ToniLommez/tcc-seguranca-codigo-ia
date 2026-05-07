#pragma once

#include <filesystem>
#include <string>

struct AppConfig {
    int port{8080};
    std::string jwt_secret{"development-secret-change-me"};
    std::filesystem::path frontend_dir{"frontend"};
    std::filesystem::path uploads_dir{"storage/uploads"};
    std::filesystem::path songs_data_path{"storage/data/songs.json"};
    std::filesystem::path firebase_service_account_path{
        "C:/Users/tonil/Desktop/tcc/lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
    };
    std::string firebase_project_id{"lead-manager-54595"};

    static AppConfig load(const std::filesystem::path& config_path);
};

