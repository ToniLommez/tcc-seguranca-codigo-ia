#include "music_handler.h"
#include "auth/jwt_helper.h"
#include "config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <ctime>
#include <random>
#include <iostream>

namespace fs = std::filesystem;

static void json_response(httplib::Response& res, int status, const nlohmann::json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

static bool extractAuth(const httplib::Request& req, std::string& userId, std::string& userType) {
    auto auth = req.get_header_value("Authorization");
    if (auth.size() < 8 || auth.substr(0, 7) != "Bearer ") return false;
    nlohmann::json payload;
    if (!JwtHelper::validateToken(auth.substr(7), Config::JWT_SECRET, payload)) return false;
    userId   = payload.value("sub", "");
    userType = payload.value("type", "");
    return !userId.empty();
}

static std::string generateId() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(now);
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str().substr(0, 24);
}

static nlohmann::json songToJson(const SongRecord& s) {
    return {
        {"id",          s.id},
        {"title",       s.title},
        {"genre",       s.genre},
        {"description", s.description},
        {"artistId",    s.artistId},
        {"artistName",  s.artistName},
        {"fileName",    s.fileName},
        {"createdAt",   s.createdAt}
    };
}

// --------------------------------------------------------------------------
MusicHandler::MusicHandler(LocalDb& db) : m_db(db) {}

void MusicHandler::registerRoutes(httplib::Server& svr) {
    svr.Post("/api/music/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleUpload(req, res);
    });
    svr.Get("/api/music", [this](const httplib::Request& req, httplib::Response& res) {
        handleList(req, res);
    });
    svr.Get("/api/music/search", [this](const httplib::Request& req, httplib::Response& res) {
        handleSearch(req, res);
    });
    svr.Get("/api/music/my-songs", [this](const httplib::Request& req, httplib::Response& res) {
        handleArtistSongs(req, res);
    });
}

void MusicHandler::handleUpload(const httplib::Request& req, httplib::Response& res) {
    std::string userId, userType;
    if (!extractAuth(req, userId, userType)) {
        json_response(res, 401, {{"error", "Não autorizado"}}); return;
    }
    if (userType != "ARTISTA") {
        json_response(res, 403, {{"error", "Apenas artistas podem fazer upload"}}); return;
    }

    // Multipart fields (text fields are in req.form.fields, files in req.form.files)
    auto getField = [&](const std::string& name) -> std::string {
        auto it = req.form.fields.find(name);
        if (it != req.form.fields.end()) return it->second.content;
        // fallback: check files map (some clients send text as file parts)
        auto it2 = req.form.files.find(name);
        return (it2 != req.form.files.end()) ? it2->second.content : "";
    };

    std::string title       = getField("title");
    std::string genre       = getField("genre");
    std::string description = getField("description");
    std::string artistName  = getField("artistName");

    if (title.empty() || genre.empty()) {
        json_response(res, 400, {{"error", "title e genre são obrigatórios"}}); return;
    }

    // File
    auto fit = req.form.files.find("file");
    if (fit == req.form.files.end()) {
        json_response(res, 400, {{"error", "Arquivo de música não enviado"}}); return;
    }

    auto& mf = fit->second;

    // Validate MP3
    bool isMp3 = mf.content_type == "audio/mpeg" ||
                 mf.content_type == "audio/mp3"  ||
                 (!mf.filename.empty() &&
                  mf.filename.size() >= 4 &&
                  mf.filename.substr(mf.filename.size() - 4) == ".mp3");
    if (!isMp3) {
        json_response(res, 400, {{"error", "Apenas arquivos MP3 são aceitos"}}); return;
    }

    // Ensure uploads dir exists
    fs::create_directories(Config::UPLOADS_DIR);

    std::string songId  = generateId();
    std::string outName = songId + ".mp3";
    std::string outPath = Config::UPLOADS_DIR + "/" + outName;

    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        json_response(res, 500, {{"error", "Erro ao salvar arquivo"}}); return;
    }
    out.write(mf.content.data(), (std::streamsize)mf.content.size());
    out.close();

    SongRecord song;
    song.id          = songId;
    song.title       = title;
    song.genre       = genre;
    song.description = description;
    song.artistId    = userId;
    song.artistName  = artistName.empty() ? "Artista" : artistName;
    song.filePath    = outPath;
    song.fileName    = outName;
    song.createdAt   = (long long)std::time(nullptr);

    if (!m_db.insertSong(song)) {
        fs::remove(outPath);
        json_response(res, 500, {{"error", "Erro ao salvar no banco de dados"}}); return;
    }

    json_response(res, 201, {{"message", "Música enviada com sucesso"}, {"id", songId}});
}

void MusicHandler::handleList(const httplib::Request& req, httplib::Response& res) {
    std::string userId, userType;
    if (!extractAuth(req, userId, userType)) {
        json_response(res, 401, {{"error", "Não autorizado"}}); return;
    }
    if (userType != "USUARIO") {
        json_response(res, 403, {{"error", "Acesso negado"}}); return;
    }

    auto songs = m_db.getAllSongs();
    nlohmann::json arr = nlohmann::json::array();
    for (auto& s : songs) arr.push_back(songToJson(s));
    json_response(res, 200, arr);
}

void MusicHandler::handleSearch(const httplib::Request& req, httplib::Response& res) {
    std::string userId, userType;
    if (!extractAuth(req, userId, userType)) {
        json_response(res, 401, {{"error", "Não autorizado"}}); return;
    }
    if (userType != "USUARIO") {
        json_response(res, 403, {{"error", "Acesso negado"}}); return;
    }

    std::string title  = req.has_param("title")  ? req.get_param_value("title")  : "";
    std::string artist = req.has_param("artist") ? req.get_param_value("artist") : "";
    std::string genre  = req.has_param("genre")  ? req.get_param_value("genre")  : "";

    auto songs = m_db.searchSongs(title, artist, genre);
    nlohmann::json arr = nlohmann::json::array();
    for (auto& s : songs) arr.push_back(songToJson(s));
    json_response(res, 200, arr);
}

void MusicHandler::handleArtistSongs(const httplib::Request& req, httplib::Response& res) {
    std::string userId, userType;
    if (!extractAuth(req, userId, userType)) {
        json_response(res, 401, {{"error", "Não autorizado"}}); return;
    }
    if (userType != "ARTISTA") {
        json_response(res, 403, {{"error", "Acesso negado"}}); return;
    }

    auto songs = m_db.getSongsByArtist(userId);
    nlohmann::json arr = nlohmann::json::array();
    for (auto& s : songs) arr.push_back(songToJson(s));
    json_response(res, 200, arr);
}
