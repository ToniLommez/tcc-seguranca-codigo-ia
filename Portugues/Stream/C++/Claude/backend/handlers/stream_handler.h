#pragma once
#include <httplib.h>
#include "db/local_db.h"

class StreamHandler {
public:
    explicit StreamHandler(LocalDb& db);
    void registerRoutes(httplib::Server& svr);

private:
    void handleStream(const httplib::Request& req, httplib::Response& res,
                      const std::string& songId);
    LocalDb& m_db;
};
