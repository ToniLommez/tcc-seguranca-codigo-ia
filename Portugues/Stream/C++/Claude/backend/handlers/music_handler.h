#pragma once
#include <httplib.h>
#include "db/local_db.h"

class MusicHandler {
public:
    explicit MusicHandler(LocalDb& db);
    void registerRoutes(httplib::Server& svr);

private:
    void handleUpload(const httplib::Request& req, httplib::Response& res);
    void handleList(const httplib::Request& req, httplib::Response& res);
    void handleSearch(const httplib::Request& req, httplib::Response& res);
    void handleArtistSongs(const httplib::Request& req, httplib::Response& res);

    LocalDb& m_db;
};
