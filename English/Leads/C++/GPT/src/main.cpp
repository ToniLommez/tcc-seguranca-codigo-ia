#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "lead_manager/app_config.hpp"
#include "lead_manager/auth_service.hpp"
#include "lead_manager/firestore_client.hpp"
#include "lead_manager/lead_service.hpp"
#include "lead_manager/models.hpp"

using lead_manager::AppConfig;
using lead_manager::AuthService;
using lead_manager::AuthenticatedUser;
using lead_manager::FirestoreClient;
using lead_manager::Lead;
using lead_manager::LeadQueryParams;
using lead_manager::LeadService;
using lead_manager::User;
using lead_manager::json;

namespace {

json errorResponse(const std::string& message) {
  return {{"success", false}, {"message", message}};
}

json successResponse(const json& data, const std::string& message = "") {
  return {{"success", true}, {"message", message}, {"data", data}};
}

void writeJson(httplib::Response& response, int status, const json& payload) {
  response.status = status;
  response.set_content(payload.dump(), "application/json");
}

json parseBody(const httplib::Request& request) {
  const auto parsed = json::parse(request.body, nullptr, false);
  if (parsed.is_discarded()) {
    throw std::runtime_error("Invalid JSON request body.");
  }
  return parsed;
}

bool requireAuth(const httplib::Request& request,
                 httplib::Response& response,
                 const AuthService& authService,
                 AuthenticatedUser& user) {
  const auto header = request.get_header_value("Authorization");
  const auto authenticated = authService.authenticateHeader(header);
  if (!authenticated.has_value()) {
    writeJson(response, 401, errorResponse("Unauthorized."));
    return false;
  }
  user = authenticated.value();
  return true;
}

std::string paramOrDefault(const httplib::Request& request, const std::string& key, const std::string& fallback = "") {
  return request.has_param(key) ? request.get_param_value(key) : fallback;
}

int intParamOrDefault(const httplib::Request& request, const std::string& key, int fallback) {
  if (!request.has_param(key)) {
    return fallback;
  }
  try {
    return std::stoi(request.get_param_value(key));
  } catch (...) {
    return fallback;
  }
}

}  // namespace

int main() {
  try {
    AppConfig config = AppConfig::load();
    if (!std::filesystem::exists(config.frontendPath)) {
      const auto candidate = std::filesystem::current_path() / "frontend";
      config.frontendPath = candidate.string();
    }

    FirestoreClient firestore(config);
    AuthService authService(config, firestore);
    LeadService leadService(firestore);

    httplib::Server server;
    server.set_mount_point("/", config.frontendPath);

    server.Get("/health", [](const httplib::Request&, httplib::Response& response) {
      writeJson(response, 200, {{"success", true}, {"message", "ok"}});
    });

    server.Post("/api/auth/register", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        const auto payload = parseBody(request);
        User user;
        std::string error;
        if (!authService.registerUser(payload.value("name", ""),
                                      payload.value("email", ""),
                                      payload.value("password", ""),
                                      user,
                                      error)) {
          writeJson(response, 400, errorResponse(error));
          return;
        }

        std::string token;
        std::string loginError;
        User loginUser;
        authService.login(user.email, payload.value("password", ""), token, loginUser, loginError);

        writeJson(response,
                  201,
                  successResponse({{"token", token}, {"user", lead_manager::userToPublicJson(loginUser)}},
                                  "User registered successfully."));
      } catch (const std::exception& exception) {
        writeJson(response, 400, errorResponse(exception.what()));
      }
    });

    server.Post("/api/auth/login", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        const auto payload = parseBody(request);
        std::string token;
        User user;
        std::string error;
        if (!authService.login(payload.value("email", ""), payload.value("password", ""), token, user, error)) {
          writeJson(response, 401, errorResponse(error));
          return;
        }

        writeJson(response,
                  200,
                  successResponse({{"token", token}, {"user", lead_manager::userToPublicJson(user)}},
                                  "Login successful."));
      } catch (const std::exception& exception) {
        writeJson(response, 400, errorResponse(exception.what()));
      }
    });

    server.Get("/api/auth/me", [&](const httplib::Request& request, httplib::Response& response) {
      AuthenticatedUser sessionUser;
      if (!requireAuth(request, response, authService, sessionUser)) {
        return;
      }

      const auto user = firestore.getUserById(sessionUser.id);
      if (!user.has_value()) {
        writeJson(response, 404, errorResponse("User not found."));
        return;
      }
      writeJson(response, 200, successResponse(lead_manager::userToPublicJson(user.value())));
    });

    server.Get("/api/dashboard/summary", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }
        writeJson(response, 200, successResponse(leadService.dashboardSummary(sessionUser.id)));
      } catch (const std::exception& exception) {
        writeJson(response, 500, errorResponse(exception.what()));
      }
    });

    server.Get("/api/leads", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }

        LeadQueryParams params;
        params.page = intParamOrDefault(request, "page", 1);
        params.pageSize = intParamOrDefault(request, "pageSize", 10);
        params.sortBy = paramOrDefault(request, "sortBy", "captureDate");
        params.sortDir = paramOrDefault(request, "sortDir", "desc");
        params.status = paramOrDefault(request, "status");
        params.source = paramOrDefault(request, "source");
        params.captureDateFrom = paramOrDefault(request, "captureDateFrom");
        params.captureDateTo = paramOrDefault(request, "captureDateTo");
        params.search = paramOrDefault(request, "search");

        writeJson(response, 200, successResponse(lead_manager::leadListToJson(leadService.listLeads(sessionUser.id, params))));
      } catch (const std::exception& exception) {
        writeJson(response, 500, errorResponse(exception.what()));
      }
    });

    server.Post("/api/leads", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }

        Lead lead;
        std::string error;
        if (!leadService.createLead(sessionUser.id, parseBody(request), lead, error)) {
          writeJson(response, 400, errorResponse(error));
          return;
        }

        writeJson(response, 201, successResponse(lead_manager::leadToJson(lead), "Lead created successfully."));
      } catch (const std::exception& exception) {
        writeJson(response, 400, errorResponse(exception.what()));
      }
    });

    server.Get(R"(/api/leads/([0-9a-f]+))", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }

        const auto lead = leadService.getLead(sessionUser.id, request.matches[1]);
        if (!lead.has_value()) {
          writeJson(response, 404, errorResponse("Lead not found."));
          return;
        }
        writeJson(response, 200, successResponse(lead_manager::leadToJson(lead.value())));
      } catch (const std::exception& exception) {
        writeJson(response, 500, errorResponse(exception.what()));
      }
    });

    server.Put(R"(/api/leads/([0-9a-f]+))", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }

        Lead lead;
        std::string error;
        if (!leadService.updateLead(sessionUser.id, request.matches[1], parseBody(request), lead, error)) {
          writeJson(response, 400, errorResponse(error));
          return;
        }
        writeJson(response, 200, successResponse(lead_manager::leadToJson(lead), "Lead updated successfully."));
      } catch (const std::exception& exception) {
        writeJson(response, 400, errorResponse(exception.what()));
      }
    });

    server.Delete(R"(/api/leads/([0-9a-f]+))", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }
        if (!leadService.deleteLead(sessionUser.id, request.matches[1])) {
          writeJson(response, 404, errorResponse("Lead not found."));
          return;
        }
        writeJson(response, 200, successResponse(json::object(), "Lead deleted successfully."));
      } catch (const std::exception& exception) {
        writeJson(response, 500, errorResponse(exception.what()));
      }
    });

    server.Get(R"(/api/leads/([0-9a-f]+)/interactions)",
               [&](const httplib::Request& request, httplib::Response& response) {
                 try {
                   AuthenticatedUser sessionUser;
                   if (!requireAuth(request, response, authService, sessionUser)) {
                     return;
                   }
                   const auto lead = leadService.getLead(sessionUser.id, request.matches[1]);
                   if (!lead.has_value()) {
                     writeJson(response, 404, errorResponse("Lead not found."));
                     return;
                   }
                   writeJson(response, 200, successResponse(lead_manager::leadToJson(lead.value()).at("interactions")));
                 } catch (const std::exception& exception) {
                   writeJson(response, 500, errorResponse(exception.what()));
                 }
               });

    server.Post(R"(/api/leads/([0-9a-f]+)/interactions)",
                [&](const httplib::Request& request, httplib::Response& response) {
                  try {
                    AuthenticatedUser sessionUser;
                    if (!requireAuth(request, response, authService, sessionUser)) {
                      return;
                    }
                    Lead lead;
                    std::string error;
                    if (!leadService.addInteraction(sessionUser.id, request.matches[1], parseBody(request), lead, error)) {
                      writeJson(response, 400, errorResponse(error));
                      return;
                    }
                    writeJson(response,
                              201,
                              successResponse(lead_manager::leadToJson(lead).at("interactions"),
                                              "Interaction saved successfully."));
                  } catch (const std::exception& exception) {
                    writeJson(response, 400, errorResponse(exception.what()));
                  }
                });

    server.Post("/api/leads/import", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }

        if (!request.has_file("file")) {
          writeJson(response, 400, errorResponse("A file is required."));
          return;
        }

        const auto file = request.get_file_value("file");
        int importedCount = 0;
        std::string error;
        if (!leadService.importLeads(sessionUser.id, file.filename, file.content, importedCount, error)) {
          writeJson(response, 400, errorResponse(error));
          return;
        }
        writeJson(response,
                  200,
                  successResponse({{"importedCount", importedCount}}, "Lead import completed successfully."));
      } catch (const std::exception& exception) {
        writeJson(response, 400, errorResponse(exception.what()));
      }
    });

    server.Get("/api/leads/export", [&](const httplib::Request& request, httplib::Response& response) {
      try {
        AuthenticatedUser sessionUser;
        if (!requireAuth(request, response, authService, sessionUser)) {
          return;
        }

        const std::string format = paramOrDefault(request, "format", "csv");
        if (format == "excel") {
          response.set_header("Content-Disposition", "attachment; filename=leads.xls");
          response.set_content(leadService.exportExcelXml(sessionUser.id), "application/vnd.ms-excel");
          return;
        }

        response.set_header("Content-Disposition", "attachment; filename=leads.csv");
        response.set_content(leadService.exportCsv(sessionUser.id), "text/csv");
      } catch (const std::exception& exception) {
        writeJson(response, 500, errorResponse(exception.what()));
      }
    });

    std::cout << "Lead Manager is running on http://localhost:" << config.port << '\n';
    std::cout << "Using Firestore project: " << config.firestoreProjectId << '\n';
    server.listen(config.host, config.port);
  } catch (const std::exception& exception) {
    std::cerr << "Startup failed: " << exception.what() << '\n';
    return 1;
  }

  return 0;
}
