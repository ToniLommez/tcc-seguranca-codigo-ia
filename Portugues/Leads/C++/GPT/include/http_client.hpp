#pragma once

#include <map>
#include <string>

struct HttpResponse {
    int status = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

HttpResponse http_request(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers = {},
    const std::string& body = {}
);

