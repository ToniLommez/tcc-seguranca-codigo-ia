#include "stream_handler.h"
#include "auth/jwt_helper.h"
#include "config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

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

StreamHandler::StreamHandler(LocalDb& db) : m_db(db) {}

void StreamHandler::registerRoutes(httplib::Server& svr) {
    svr.Get(R"(/api/music/([^/]+)/stream)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string songId = req.matches[1];
        handleStream(req, res, songId);
    });
}

void StreamHandler::handleStream(const httplib::Request& req, httplib::Response& res,
                                  const std::string& songId) {
    std::string userId, userType;
    if (!extractAuth(req, userId, userType)) {
        json_response(res, 401, {{"error", "Não autorizado"}}); return;
    }
    if (userType != "USUARIO") {
        json_response(res, 403, {{"error", "Apenas usuários podem ouvir músicas"}}); return;
    }

    auto songOpt = m_db.getSongById(songId);
    if (!songOpt) {
        json_response(res, 404, {{"error", "Música não encontrada"}}); return;
    }

    std::ifstream file(songOpt->filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        json_response(res, 404, {{"error", "Arquivo não encontrado no servidor"}}); return;
    }

    long long file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    long long range_start = 0;
    long long range_end   = file_size - 1;
    bool      is_range    = false;

    std::string range_hdr = req.get_header_value("Range");
    if (!range_hdr.empty()) {
        // Parse "bytes=start-end" or "bytes=start-"
        long long s = 0, e = -1;
        if (sscanf(range_hdr.c_str(), "bytes=%lld-%lld", &s, &e) >= 1) {
            range_start = s;
            range_end   = (e >= 0) ? e : file_size - 1;
            is_range    = true;
        }
    }

    if (range_start < 0 || range_start >= file_size) {
        res.status = 416;
        res.set_header("Content-Range", "bytes */" + std::to_string(file_size));
        return;
    }
    if (range_end >= file_size) range_end = file_size - 1;

    long long length = range_end - range_start + 1;

    file.seekg(range_start);
    std::string content(length, '\0');
    file.read(content.data(), length);
    file.close();

    res.set_header("Accept-Ranges", "bytes");
    res.set_header("Content-Length", std::to_string(length));
    res.set_header("Cache-Control", "no-cache");

    if (is_range) {
        res.status = 206;
        res.set_header("Content-Range",
            "bytes " + std::to_string(range_start) + "-" +
            std::to_string(range_end) + "/" + std::to_string(file_size));
    } else {
        res.status = 200;
    }

    res.set_content(content, "audio/mpeg");
}
