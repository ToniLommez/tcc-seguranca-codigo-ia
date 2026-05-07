#pragma once

#include <map>
#include <string>

namespace lead_manager {

struct HttpResponse {
  int status = 0;
  std::string body;
  std::string error;

  [[nodiscard]] bool ok() const {
    return error.empty() && status >= 200 && status < 300;
  }
};

class HttpClient {
 public:
  HttpResponse request(const std::string& method,
                       const std::string& url,
                       const std::map<std::string, std::string>& headers = {},
                       const std::string& body = "",
                       const std::string& contentType = "application/json") const;
};

}  // namespace lead_manager

