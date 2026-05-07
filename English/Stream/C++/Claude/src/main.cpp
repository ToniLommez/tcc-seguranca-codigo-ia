#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "models.h"
#include "crypto_utils.h"
#include "jwt_auth.h"
#include "firebase_client.h"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace soundstream;

static const std::string JWT_SECRET = "soundstream-secret-key-change-in-production-2024";
static const std::string UPLOADS_DIR = "uploads";
static const std::string FRONTEND_DIR = "frontend";
static const std::string FIREBASE_CREDENTIALS = "firebase-credentials.json";

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

static std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static json error_response(const std::string& message, int status = 400) {
    return {{"error", true}, {"message", message}, {"status", status}};
}

static json success_response(const std::string& message, const json& data = nullptr) {
    json resp = {{"error", false}, {"message", message}};
    if (!data.is_null()) resp["data"] = data;
    return resp;
}

static bool authenticate(const httplib::Request& req, JwtAuth& jwt, TokenPayload& payload) {
    std::string token;

    auto auth = req.get_header_value("Authorization");
    if (!auth.empty() && auth.substr(0, 7) == "Bearer ") {
        token = auth.substr(7);
    }

    if (token.empty()) {
        token = req.get_param_value("token");
    }

    if (token.empty()) return false;

    try {
        payload = jwt.verify_token(token);
        return true;
    } catch (...) {
        return false;
    }
}

static void setup_cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

int main() {
    std::cout << "=== SoundStream Music Server ===" << std::endl;

    if (!fs::exists(UPLOADS_DIR)) fs::create_directories(UPLOADS_DIR);
    if (!fs::exists(FRONTEND_DIR)) {
        std::cerr << "Warning: frontend directory not found at: " << fs::absolute(FRONTEND_DIR) << std::endl;
    }

    std::string creds_path = FIREBASE_CREDENTIALS;
    if (!fs::exists(creds_path)) {
        std::cerr << "Firebase credentials not found at: " << creds_path << std::endl;
        return 1;
    }

    std::cout << "[Init] Connecting to Firebase..." << std::endl;
    FirebaseClient firebase(creds_path);
    JwtAuth jwt(JWT_SECRET);
    std::cout << "[Init] Firebase connected." << std::endl;

    httplib::Server svr;

    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        setup_cors(res);
        res.status = 204;
    });

    // ==================== AUTH ROUTES ====================

    svr.Post("/api/auth/register", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);
        res.set_header("Content-Type", "application/json");

        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(error_response("Invalid JSON body").dump(), "application/json");
            return;
        }

        if (!body.contains("name") || !body.contains("email") ||
            !body.contains("password") || !body.contains("type")) {
            res.status = 400;
            res.set_content(error_response("Missing required fields: name, email, password, type").dump(),
                           "application/json");
            return;
        }

        std::string name = body["name"].get<std::string>();
        std::string email = body["email"].get<std::string>();
        std::string password = body["password"].get<std::string>();
        std::string type = body["type"].get<std::string>();

        if (type != "ARTIST" && type != "USER") {
            res.status = 400;
            res.set_content(error_response("Type must be ARTIST or USER").dump(), "application/json");
            return;
        }

        if (password.length() < 6) {
            res.status = 400;
            res.set_content(error_response("Password must be at least 6 characters").dump(), "application/json");
            return;
        }

        auto existing = firebase.query_by_field("users", "email", email);
        if (!existing.empty()) {
            res.status = 409;
            res.set_content(error_response("Email already registered").dump(), "application/json");
            return;
        }

        std::string salt = generate_salt();
        std::string password_hash = hash_password(password, salt);
        std::string user_id = generate_uuid();

        json user_data = {
            {"name", name},
            {"email", email},
            {"password_hash", password_hash},
            {"salt", salt},
            {"type", type},
            {"created_at", get_timestamp()}
        };

        auto result = firebase.create_document("users", user_id, user_data);
        if (result.contains("error")) {
            res.status = 500;
            res.set_content(error_response("Failed to create user").dump(), "application/json");
            return;
        }

        TokenPayload tp{user_id, email, type, name};
        std::string token = jwt.create_token(tp);

        json response_data = {
            {"token", token},
            {"user", {{"id", user_id}, {"name", name}, {"email", email}, {"type", type}}}
        };

        res.status = 201;
        res.set_content(success_response("User registered successfully", response_data).dump(),
                       "application/json");
        std::cout << "[Auth] User registered: " << email << " (" << type << ")" << std::endl;
    });

    svr.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);
        res.set_header("Content-Type", "application/json");

        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(error_response("Invalid JSON body").dump(), "application/json");
            return;
        }

        if (!body.contains("email") || !body.contains("password")) {
            res.status = 400;
            res.set_content(error_response("Missing email or password").dump(), "application/json");
            return;
        }

        std::string email = body["email"].get<std::string>();
        std::string password = body["password"].get<std::string>();

        auto users = firebase.query_by_field("users", "email", email);
        if (users.empty()) {
            res.status = 401;
            res.set_content(error_response("Invalid email or password").dump(), "application/json");
            return;
        }

        auto user = users[0];
        std::string stored_hash = user["password_hash"].get<std::string>();
        std::string salt = user["salt"].get<std::string>();

        if (!verify_password(password, salt, stored_hash)) {
            res.status = 401;
            res.set_content(error_response("Invalid email or password").dump(), "application/json");
            return;
        }

        TokenPayload tp{
            user["id"].get<std::string>(),
            email,
            user["type"].get<std::string>(),
            user["name"].get<std::string>()
        };
        std::string token = jwt.create_token(tp);

        json response_data = {
            {"token", token},
            {"user", {
                {"id", user["id"]},
                {"name", user["name"]},
                {"email", email},
                {"type", user["type"]}
            }}
        };

        res.status = 200;
        res.set_content(success_response("Login successful", response_data).dump(), "application/json");
        std::cout << "[Auth] User logged in: " << email << std::endl;
    });

    // ==================== ARTIST ROUTES ====================

    svr.Post("/api/artist/songs", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);
        res.set_header("Content-Type", "application/json");

        TokenPayload tp;
        if (!authenticate(req, jwt, tp)) {
            res.status = 401;
            res.set_content(error_response("Authentication required").dump(), "application/json");
            return;
        }

        if (tp.type != "ARTIST") {
            res.status = 403;
            res.set_content(error_response("Only artists can upload songs").dump(), "application/json");
            return;
        }

        if (!req.form.has_file("file")) {
            res.status = 400;
            res.set_content(error_response("Music file is required").dump(), "application/json");
            return;
        }

        auto file = req.form.get_file("file");
        std::string filename = file.filename;

        std::string ext = filename.substr(filename.find_last_of('.'));
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".mp3") {
            res.status = 400;
            res.set_content(error_response("Only MP3 files are allowed").dump(), "application/json");
            return;
        }

        std::string title = req.form.has_field("title") ? req.form.get_field("title") : "";
        std::string genre = req.form.has_field("genre") ? req.form.get_field("genre") : "";
        std::string description = req.form.has_field("description") ? req.form.get_field("description") : "";

        if (title.empty()) {
            res.status = 400;
            res.set_content(error_response("Song title is required").dump(), "application/json");
            return;
        }

        std::string song_id = generate_uuid();
        std::string safe_filename = song_id + ".mp3";
        std::string file_path = UPLOADS_DIR + "/" + safe_filename;

        std::ofstream ofs(file_path, std::ios::binary);
        if (!ofs.is_open()) {
            res.status = 500;
            res.set_content(error_response("Failed to save file").dump(), "application/json");
            return;
        }
        ofs.write(file.content.c_str(), file.content.size());
        ofs.close();

        json song_data = {
            {"title", title},
            {"genre", genre},
            {"description", description},
            {"file_path", file_path},
            {"artist_id", tp.user_id},
            {"artist_name", tp.name},
            {"created_at", get_timestamp()}
        };

        auto result = firebase.create_document("songs", song_id, song_data);
        if (result.contains("error") && !result.contains("name")) {
            fs::remove(file_path);
            res.status = 500;
            res.set_content(error_response("Failed to save song metadata").dump(), "application/json");
            return;
        }

        json response_data = {
            {"id", song_id}, {"title", title}, {"genre", genre},
            {"description", description}, {"artist_name", tp.name},
            {"created_at", song_data["created_at"]}
        };

        res.status = 201;
        res.set_content(success_response("Song uploaded successfully", response_data).dump(),
                       "application/json");
        std::cout << "[Artist] Song uploaded: " << title << " by " << tp.name << std::endl;
    });

    svr.Get("/api/artist/songs", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);
        res.set_header("Content-Type", "application/json");

        TokenPayload tp;
        if (!authenticate(req, jwt, tp)) {
            res.status = 401;
            res.set_content(error_response("Authentication required").dump(), "application/json");
            return;
        }

        if (tp.type != "ARTIST") {
            res.status = 403;
            res.set_content(error_response("Only artists can access this endpoint").dump(), "application/json");
            return;
        }

        auto songs = firebase.query_by_field("songs", "artist_id", tp.user_id);

        res.status = 200;
        res.set_content(success_response("Songs retrieved", songs).dump(), "application/json");
    });

    // ==================== USER ROUTES ====================

    svr.Get("/api/songs", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);
        res.set_header("Content-Type", "application/json");

        TokenPayload tp;
        if (!authenticate(req, jwt, tp)) {
            res.status = 401;
            res.set_content(error_response("Authentication required").dump(), "application/json");
            return;
        }

        if (tp.type != "USER") {
            res.status = 403;
            res.set_content(error_response("Only regular users can access this endpoint").dump(),
                           "application/json");
            return;
        }

        auto songs = firebase.list_documents("songs");

        for (auto& song : songs) {
            song.erase("file_path");
            song.erase("password_hash");
            song.erase("salt");
        }

        res.status = 200;
        res.set_content(success_response("Songs retrieved", songs).dump(), "application/json");
    });

    svr.Get("/api/songs/search", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);
        res.set_header("Content-Type", "application/json");

        TokenPayload tp;
        if (!authenticate(req, jwt, tp)) {
            res.status = 401;
            res.set_content(error_response("Authentication required").dump(), "application/json");
            return;
        }

        if (tp.type != "USER") {
            res.status = 403;
            res.set_content(error_response("Only regular users can search songs").dump(), "application/json");
            return;
        }

        std::string query = req.get_param_value("q");
        std::string genre = req.get_param_value("genre");
        std::string artist = req.get_param_value("artist");

        auto all_songs = firebase.list_documents("songs");
        json filtered = json::array();

        std::string q_lower = to_lower(query);
        std::string genre_lower = to_lower(genre);
        std::string artist_lower = to_lower(artist);

        for (auto& song : all_songs) {
            bool match = true;

            if (!query.empty()) {
                std::string title = to_lower(song.value("title", ""));
                if (title.find(q_lower) == std::string::npos) match = false;
            }

            if (match && !genre.empty()) {
                std::string song_genre = to_lower(song.value("genre", ""));
                if (song_genre.find(genre_lower) == std::string::npos) match = false;
            }

            if (match && !artist.empty()) {
                std::string song_artist = to_lower(song.value("artist_name", ""));
                if (song_artist.find(artist_lower) == std::string::npos) match = false;
            }

            if (match) {
                song.erase("file_path");
                filtered.push_back(song);
            }
        }

        res.status = 200;
        res.set_content(success_response("Search results", filtered).dump(), "application/json");
    });

    // ==================== STREAMING ROUTE ====================

    svr.Get(R"(/api/stream/([a-f0-9\-]+))", [&](const httplib::Request& req, httplib::Response& res) {
        setup_cors(res);

        TokenPayload tp;
        if (!authenticate(req, jwt, tp)) {
            res.status = 401;
            res.set_header("Content-Type", "application/json");
            res.set_content(error_response("Authentication required").dump(), "application/json");
            return;
        }

        if (tp.type != "USER") {
            res.status = 403;
            res.set_header("Content-Type", "application/json");
            res.set_content(error_response("Only regular users can stream music").dump(), "application/json");
            return;
        }

        std::string song_id = req.matches[1];
        auto song = firebase.get_document("songs", song_id);

        if (song.contains("error") || !song.contains("file_path")) {
            res.status = 404;
            res.set_header("Content-Type", "application/json");
            res.set_content(error_response("Song not found").dump(), "application/json");
            return;
        }

        std::string file_path = song["file_path"].get<std::string>();

        if (!fs::exists(file_path)) {
            res.status = 404;
            res.set_header("Content-Type", "application/json");
            res.set_content(error_response("Audio file not found on server").dump(), "application/json");
            return;
        }

        auto file_size = static_cast<int64_t>(fs::file_size(file_path));
        std::string range_header = req.get_header_value("Range");

        if (!range_header.empty() && range_header.substr(0, 6) == "bytes=") {
            std::string range_spec = range_header.substr(6);
            int64_t start = 0, end = file_size - 1;

            auto dash_pos = range_spec.find('-');
            if (dash_pos != std::string::npos) {
                std::string start_str = range_spec.substr(0, dash_pos);
                std::string end_str = range_spec.substr(dash_pos + 1);

                if (!start_str.empty()) start = std::stoll(start_str);
                if (!end_str.empty()) end = std::stoll(end_str);
            }

            if (start >= file_size) {
                res.status = 416;
                res.set_header("Content-Range", "bytes */" + std::to_string(file_size));
                return;
            }

            if (end >= file_size) end = file_size - 1;
            int64_t content_length = end - start + 1;

            std::ifstream ifs(file_path, std::ios::binary);
            ifs.seekg(start);
            std::vector<char> buffer(content_length);
            ifs.read(buffer.data(), content_length);

            res.status = 206;
            res.set_header("Content-Type", "audio/mpeg");
            res.set_header("Content-Range",
                          "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size));
            res.set_header("Accept-Ranges", "bytes");
            res.set_header("Content-Length", std::to_string(content_length));
            res.set_content(std::string(buffer.begin(), buffer.end()), "audio/mpeg");
        } else {
            std::ifstream ifs(file_path, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());

            res.status = 200;
            res.set_header("Content-Type", "audio/mpeg");
            res.set_header("Accept-Ranges", "bytes");
            res.set_header("Content-Length", std::to_string(file_size));
            res.set_content(content, "audio/mpeg");
        }
    });

    // ==================== STATIC FILES (FRONTEND) ====================

    svr.set_mount_point("/", FRONTEND_DIR);

    // ==================== START SERVER ====================

    const std::string HOST = "0.0.0.0";
    const int PORT = 8080;

    std::cout << "\n========================================" << std::endl;
    std::cout << "  SoundStream Server" << std::endl;
    std::cout << "  http://localhost:" << PORT << std::endl;
    std::cout << "========================================\n" << std::endl;

    if (!svr.listen(HOST, PORT)) {
        std::cerr << "Failed to start server on " << HOST << ":" << PORT << std::endl;
        return 1;
    }

    return 0;
}
