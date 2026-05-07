#pragma once
#include <string>

// ============================================================
// Server Configuration
// ============================================================
constexpr int    SERVER_PORT = 8080;
constexpr const char* SERVER_HOST = "0.0.0.0";

// ============================================================
// JWT Configuration
// ============================================================
constexpr const char* JWT_SECRET = "lead_manager_s3cr3t_key_CHANGE_IN_PRODUCTION_2024";
constexpr int    JWT_EXPIRY_HOURS = 24;

// ============================================================
// Firebase Configuration
// ============================================================
constexpr const char* FIREBASE_PROJECT_ID      = "lead-manager-54595";
constexpr const char* FIREBASE_CREDENTIALS_PATH =
    "firebase-credentials.json";  // copied next to executable by CMake

// ============================================================
// Firestore Collection Names
// Format: <model>-<language>-<collection>
// ============================================================
constexpr const char* COLLECTION_USERS = "claude-sonnet-4-6-cpp-users";
constexpr const char* COLLECTION_LEADS = "claude-sonnet-4-6-cpp-leads";

// ============================================================
// Frontend Static Files Directory
// ============================================================
constexpr const char* FRONTEND_DIR = "frontend";

// ============================================================
// Pagination Defaults
// ============================================================
constexpr int DEFAULT_PAGE_SIZE = 20;
constexpr int MAX_PAGE_SIZE     = 100;
