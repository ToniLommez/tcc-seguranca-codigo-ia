#include "server.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "../external/httplib.h"
#include "csv_utils.hpp"
#include "firebase_client.hpp"
#include "security.hpp"
#include "utils.hpp"

namespace {

const std::set<std::string> kAllowedStatuses = {
    "novo",
    "em contato",
    "qualificado",
    "perdido"
};

std::string read_file_safely(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

json public_user_json(const AuthUser& user) {
    return {
        {"id", user.id},
        {"name", user.name},
        {"email", user.email},
        {"createdAt", user.createdAt},
        {"updatedAt", user.updatedAt}
    };
}

json lead_to_json(const LeadRecord& lead) {
    return {
        {"id", lead.id},
        {"userId", lead.userId},
        {"fullName", lead.fullName},
        {"email", lead.email},
        {"phone", lead.phone},
        {"company", lead.company},
        {"jobTitle", lead.jobTitle},
        {"source", lead.source},
        {"status", lead.status},
        {"captureDate", lead.captureDate},
        {"notes", lead.notes},
        {"createdAt", lead.createdAt},
        {"updatedAt", lead.updatedAt},
        {"interactions", lead.interactions}
    };
}

void send_json(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

void send_error(httplib::Response& res, int status, const std::string& message) {
    send_json(res, status, {{"error", message}});
}

std::optional<json> parse_body_json(const httplib::Request& req, httplib::Response& res) {
    try {
        return json::parse(req.body.empty() ? "{}" : req.body);
    } catch (...) {
        send_error(res, 400, "JSON invalido");
        return std::nullopt;
    }
}

std::optional<JwtPayload> require_auth(const AppConfig& config, const httplib::Request& req, httplib::Response& res) {
    const auto header = req.get_header_value("Authorization");
    if (header.rfind("Bearer ", 0) != 0) {
        send_error(res, 401, "Token JWT ausente");
        return std::nullopt;
    }

    const auto token = header.substr(7);
    const auto payload = verify_app_jwt(config, token);
    if (!payload.has_value()) {
        send_error(res, 401, "Token JWT invalido ou expirado");
        return std::nullopt;
    }
    return payload;
}

bool validate_lead_payload(const json& body, std::string& message) {
    const auto status = normalize_search_text(body.value("status", ""));
    if (body.value("fullName", "").empty()) {
        message = "Nome completo e obrigatorio";
        return false;
    }
    if (body.value("email", "").empty()) {
        message = "E-mail e obrigatorio";
        return false;
    }
    if (!status.empty() && !kAllowedStatuses.count(status)) {
        message = "Status invalido";
        return false;
    }
    return true;
}

LeadRecord payload_to_lead(const json& body, const std::string& userId, const std::optional<LeadRecord>& existing = std::nullopt) {
    LeadRecord lead = existing.value_or(LeadRecord{});
    if (lead.id.empty()) {
        lead.id = random_id();
        lead.createdAt = now_iso8601_utc();
        lead.interactions = json::array();
    }

    lead.userId = userId;
    lead.fullName = trim(body.value("fullName", lead.fullName));
    lead.email = normalize_email(body.value("email", lead.email));
    lead.phone = trim(body.value("phone", lead.phone));
    lead.company = trim(body.value("company", lead.company));
    lead.jobTitle = trim(body.value("jobTitle", lead.jobTitle));
    lead.source = trim(body.value("source", lead.source));
    lead.status = normalize_search_text(body.value("status", lead.status));
    lead.captureDate = trim(body.value("captureDate", lead.captureDate.empty() ? today_iso8601_utc() : lead.captureDate));
    lead.notes = trim(body.value("notes", lead.notes));
    if (body.contains("interactions") && body["interactions"].is_array()) {
        lead.interactions = body["interactions"];
    }
    lead.updatedAt = now_iso8601_utc();
    if (!lead.interactions.is_array()) {
        lead.interactions = json::array();
    }
    return lead;
}

bool can_access_lead(const JwtPayload& auth, const LeadRecord& lead, httplib::Response& res) {
    if (lead.userId != auth.userId) {
        send_error(res, 403, "Lead nao pertence ao usuario autenticado");
        return false;
    }
    return true;
}

json build_dashboard_payload(const std::vector<LeadRecord>& leads) {
    std::map<std::string, int> statusCounts = {
        {"novo", 0},
        {"em contato", 0},
        {"qualificado", 0},
        {"perdido", 0}
    };
    std::map<std::string, int> capturesByDate;

    for (const auto& lead : leads) {
        statusCounts[lead.status]++;
        capturesByDate[lead.captureDate]++;
    }

    json captures = json::array();
    for (const auto& [date, total] : capturesByDate) {
        captures.push_back({{"date", date}, {"total", total}});
    }

    int total = static_cast<int>(leads.size());
    int qualified = statusCounts["qualificado"];
    int conversion = total == 0 ? 0 : static_cast<int>((qualified * 100.0) / total);

    return {
        {"totals", {
            {"totalLeads", total},
            {"qualifiedLeads", qualified},
            {"conversionRate", conversion}
        }},
        {"statusCounts", statusCounts},
        {"capturesByDate", captures}
    };
}

json build_leads_page(const std::vector<LeadRecord>& leads, int page, int pageSize) {
    const int total = static_cast<int>(leads.size());
    const int start = std::max(0, (page - 1) * pageSize);
    const int end = std::min(total, start + pageSize);

    json items = json::array();
    for (int i = start; i < end; ++i) {
        items.push_back(lead_to_json(leads[static_cast<std::size_t>(i)]));
    }

    return {
        {"items", items},
        {"pagination", {
            {"page", page},
            {"pageSize", pageSize},
            {"total", total},
            {"totalPages", pageSize == 0 ? 0 : (total + pageSize - 1) / pageSize}
        }}
    };
}

std::string param_or_empty(const httplib::Request& req, const char* name) {
    return req.has_param(name) ? req.get_param_value(name) : "";
}

void sort_leads(std::vector<LeadRecord>& leads, const std::string& sortBy, const std::string& sortDir) {
    const bool descending = to_lower(sortDir) == "desc";
    auto extract = [&](const LeadRecord& lead) {
        if (sortBy == "fullName") return to_lower(lead.fullName);
        if (sortBy == "email") return to_lower(lead.email);
        if (sortBy == "company") return to_lower(lead.company);
        if (sortBy == "status") return to_lower(lead.status);
        if (sortBy == "source") return to_lower(lead.source);
        if (sortBy == "captureDate") return lead.captureDate;
        return lead.updatedAt;
    };

    std::sort(leads.begin(), leads.end(), [&](const LeadRecord& left, const LeadRecord& right) {
        if (descending) {
            return extract(left) > extract(right);
        }
        return extract(left) < extract(right);
    });
}

std::vector<LeadRecord> filter_leads(const std::vector<LeadRecord>& leads, const httplib::Request& req) {
    auto filtered = leads;
    const auto status = normalize_search_text(param_or_empty(req, "status"));
    const auto source = normalize_search_text(param_or_empty(req, "source"));
    const auto dateFrom = trim(param_or_empty(req, "dateFrom"));
    const auto dateTo = trim(param_or_empty(req, "dateTo"));
    const auto query = normalize_search_text(param_or_empty(req, "q"));

    filtered.erase(
        std::remove_if(filtered.begin(), filtered.end(), [&](const LeadRecord& lead) {
            if (!status.empty() && normalize_search_text(lead.status) != status) {
                return true;
            }
            if (!source.empty() && normalize_search_text(lead.source) != source) {
                return true;
            }
            if (!dateFrom.empty() && lead.captureDate < dateFrom) {
                return true;
            }
            if (!dateTo.empty() && lead.captureDate > dateTo) {
                return true;
            }
            if (!query.empty()) {
                const auto haystack = normalize_search_text(lead.fullName + " " + lead.email + " " + lead.company);
                if (haystack.find(query) == std::string::npos) {
                    return true;
                }
            }
            return false;
        }),
        filtered.end()
    );

    const auto sortBy = req.has_param("sortBy") ? req.get_param_value("sortBy") : "updatedAt";
    const auto sortDir = req.has_param("sortDir") ? req.get_param_value("sortDir") : "desc";
    sort_leads(filtered, sortBy, sortDir);
    return filtered;
}

}  // namespace

void run_server(const AppConfig& config) {
    FirebaseClient firebase(config);
    std::string startupError;
    const bool firebaseOk = firebase.ping(startupError);
    if (!firebaseOk) {
        std::cerr << "Aviso: Firebase nao respondeu na inicializacao: " << startupError << std::endl;
    }

    httplib::Server server;
    server.new_task_queue = [] { return new httplib::ThreadPool(8); };

    const auto publicDir = (std::filesystem::path(get_executable_directory()) / "public").string();
    server.set_mount_point("/", publicDir.c_str());

    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404 && res.body.empty()) {
            res.set_content(R"({"error":"Rota nao encontrada"})", "application/json; charset=utf-8");
        }
    });

    server.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        send_json(res, 200, {
            {"status", "ok"},
            {"firebase", firebaseOk ? "connected" : "degraded"},
            {"collections", {
                {"users", firebase.users_collection()},
                {"leads", firebase.leads_collection()}
            }}
        });
    });

    server.Post("/api/auth/register", [&](const httplib::Request& req, httplib::Response& res) {
        const auto body = parse_body_json(req, res);
        if (!body.has_value()) {
            return;
        }

        const auto name = trim(body->value("name", ""));
        const auto email = normalize_email(body->value("email", ""));
        const auto password = body->value("password", "");
        if (name.empty() || email.empty() || password.size() < 6) {
            send_error(res, 400, "Informe nome, e-mail e senha com pelo menos 6 caracteres");
            return;
        }

        std::string errorMessage;
        const auto existing = firebase.get_user_by_email(email, errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (existing.has_value()) {
            send_error(res, 409, "Ja existe usuario cadastrado com este e-mail");
            return;
        }

        AuthUser user;
        user.id = user_document_id(email);
        user.name = name;
        user.email = email;
        user.passwordHash = hash_password(password);
        user.createdAt = now_iso8601_utc();
        user.updatedAt = user.createdAt;

        if (!firebase.create_user(user, errorMessage)) {
            send_error(res, 500, errorMessage);
            return;
        }

        const auto token = create_app_jwt(config, user);
        send_json(res, 201, {
            {"token", token},
            {"user", public_user_json(user)}
        });
    });

    server.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        const auto body = parse_body_json(req, res);
        if (!body.has_value()) {
            return;
        }

        const auto email = normalize_email(body->value("email", ""));
        const auto password = body->value("password", "");
        if (email.empty() || password.empty()) {
            send_error(res, 400, "Informe e-mail e senha");
            return;
        }

        std::string errorMessage;
        const auto user = firebase.get_user_by_email(email, errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (!user.has_value() || !verify_password(password, user->passwordHash)) {
            send_error(res, 401, "Credenciais invalidas");
            return;
        }

        send_json(res, 200, {
            {"token", create_app_jwt(config, *user)},
            {"user", public_user_json(*user)}
        });
    });

    server.Get("/api/auth/me", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto user = firebase.get_user_by_email(auth->email, errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (!user.has_value()) {
            send_error(res, 404, "Usuario nao encontrado");
            return;
        }
        send_json(res, 200, {{"user", public_user_json(*user)}});
    });

    server.Get("/api/dashboard/summary", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto leads = firebase.list_leads_for_user(auth->userId, errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        send_json(res, 200, build_dashboard_payload(leads));
    });

    server.Get("/api/leads", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto leads = firebase.list_leads_for_user(auth->userId, errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }

        const auto filtered = filter_leads(leads, req);
        int page = 1;
        int pageSize = 20;
        if (req.has_param("page")) {
            page = std::max(1, std::stoi(req.get_param_value("page")));
        }
        if (req.has_param("pageSize")) {
            pageSize = std::clamp(std::stoi(req.get_param_value("pageSize")), 1, 100);
        }
        send_json(res, 200, build_leads_page(filtered, page, pageSize));
    });

    server.Post("/api/leads", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        const auto body = parse_body_json(req, res);
        if (!body.has_value()) {
            return;
        }

        std::string validationMessage;
        if (!validate_lead_payload(*body, validationMessage)) {
            send_error(res, 400, validationMessage);
            return;
        }

        auto lead = payload_to_lead(*body, auth->userId);
        std::string errorMessage;
        if (!firebase.create_lead(lead, errorMessage)) {
            send_error(res, 500, errorMessage);
            return;
        }
        send_json(res, 201, {{"lead", lead_to_json(lead)}});
    });

    server.Get("/api/leads/export/csv", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto leads = firebase.list_leads_for_user(auth->userId, errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }

        std::vector<json> rows;
        for (const auto& lead : filter_leads(leads, req)) {
            rows.push_back(lead_to_json(lead));
        }

        res.status = 200;
        res.set_header("Content-Disposition", "attachment; filename=leads-export.csv");
        res.set_content(generate_csv(rows), "text/csv; charset=utf-8");
    });

    server.Get("/api/leads/:id", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto lead = firebase.get_lead(req.path_params.at("id"), errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (!lead.has_value()) {
            send_error(res, 404, "Lead nao encontrado");
            return;
        }
        if (!can_access_lead(*auth, *lead, res)) {
            return;
        }
        send_json(res, 200, {{"lead", lead_to_json(*lead)}});
    });

    server.Put("/api/leads/:id", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        const auto body = parse_body_json(req, res);
        if (!body.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto existing = firebase.get_lead(req.path_params.at("id"), errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (!existing.has_value()) {
            send_error(res, 404, "Lead nao encontrado");
            return;
        }
        if (!can_access_lead(*auth, *existing, res)) {
            return;
        }

        std::string validationMessage;
        if (!validate_lead_payload(*body, validationMessage)) {
            send_error(res, 400, validationMessage);
            return;
        }

        auto updated = payload_to_lead(*body, auth->userId, existing);
        updated.id = existing->id;
        updated.createdAt = existing->createdAt;
        if (!firebase.update_lead(updated, errorMessage)) {
            send_error(res, 500, errorMessage);
            return;
        }
        send_json(res, 200, {{"lead", lead_to_json(updated)}});
    });

    server.Delete("/api/leads/:id", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto lead = firebase.get_lead(req.path_params.at("id"), errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (!lead.has_value()) {
            send_error(res, 404, "Lead nao encontrado");
            return;
        }
        if (!can_access_lead(*auth, *lead, res)) {
            return;
        }
        if (!firebase.delete_lead(lead->id, errorMessage)) {
            send_error(res, 500, errorMessage);
            return;
        }
        send_json(res, 200, {{"deleted", true}});
    });

    server.Post("/api/leads/:id/interactions", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        const auto body = parse_body_json(req, res);
        if (!body.has_value()) {
            return;
        }

        std::string errorMessage;
        const auto lead = firebase.get_lead(req.path_params.at("id"), errorMessage);
        if (!errorMessage.empty()) {
            send_error(res, 500, errorMessage);
            return;
        }
        if (!lead.has_value()) {
            send_error(res, 404, "Lead nao encontrado");
            return;
        }
        if (!can_access_lead(*auth, *lead, res)) {
            return;
        }

        const auto note = trim(body->value("note", ""));
        if (note.empty()) {
            send_error(res, 400, "Informe a descricao da interacao");
            return;
        }

        json interaction = {
            {"id", random_id()},
            {"type", trim(body->value("type", "nota"))},
            {"note", note},
            {"createdAt", now_iso8601_utc()}
        };

        auto updated = *lead;
        if (!updated.interactions.is_array()) {
            updated.interactions = json::array();
        }
        updated.interactions.push_back(interaction);
        updated.updatedAt = now_iso8601_utc();

        if (!firebase.update_lead(updated, errorMessage)) {
            send_error(res, 500, errorMessage);
            return;
        }
        send_json(res, 201, {{"lead", lead_to_json(updated)}});
    });

    server.Post("/api/leads/import/json", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        const auto body = parse_body_json(req, res);
        if (!body.has_value()) {
            return;
        }
        if (!body->contains("rows") || !(*body)["rows"].is_array()) {
            send_error(res, 400, "Envie um array rows para importacao");
            return;
        }

        int imported = 0;
        std::vector<std::string> errors;
        for (const auto& row : (*body)["rows"]) {
            if (!row.is_object()) {
                continue;
            }

            json normalized = {
                {"fullName", row.value("fullName", row.value("nome_completo", row.value("nome", "")))},
                {"email", row.value("email", "")},
                {"phone", row.value("phone", row.value("telefone", ""))},
                {"company", row.value("company", row.value("empresa", ""))},
                {"jobTitle", row.value("jobTitle", row.value("cargo", ""))},
                {"source", row.value("source", row.value("fonte", ""))},
                {"status", row.value("status", "novo")},
                {"captureDate", row.value("captureDate", row.value("data_captura", today_iso8601_utc()))},
                {"notes", row.value("notes", row.value("observacoes", ""))}
            };

            std::string validationMessage;
            if (!validate_lead_payload(normalized, validationMessage)) {
                errors.push_back(validationMessage + ": " + normalized.value("fullName", "(sem nome)"));
                continue;
            }

            auto lead = payload_to_lead(normalized, auth->userId);
            std::string errorMessage;
            if (!firebase.create_lead(lead, errorMessage)) {
                errors.push_back(errorMessage + ": " + lead.fullName);
                continue;
            }
            ++imported;
        }

        send_json(res, 200, {
            {"imported", imported},
            {"errors", errors}
        });
    });

    server.Post("/api/leads/import/csv", [&](const httplib::Request& req, httplib::Response& res) {
        const auto auth = require_auth(config, req, res);
        if (!auth.has_value()) {
            return;
        }

        const auto rows = parse_csv_rows(req.body);
        if (rows.size() < 2) {
            send_error(res, 400, "CSV vazio ou sem cabecalho");
            return;
        }

        std::map<std::string, std::size_t> headerIndex;
        for (std::size_t i = 0; i < rows[0].size(); ++i) {
            headerIndex[normalize_search_text(rows[0][i])] = i;
        }

        auto column = [&](const std::vector<std::string>& row, const std::vector<std::string>& aliases) {
            for (const auto& alias : aliases) {
                const auto it = headerIndex.find(normalize_search_text(alias));
                if (it != headerIndex.end() && it->second < row.size()) {
                    return row[it->second];
                }
            }
            return std::string{};
        };

        int imported = 0;
        std::vector<std::string> errors;
        for (std::size_t line = 1; line < rows.size(); ++line) {
            const auto& row = rows[line];
            json payload = {
                {"fullName", column(row, {"nome_completo", "nome", "fullName"})},
                {"email", column(row, {"email"})},
                {"phone", column(row, {"telefone", "phone"})},
                {"company", column(row, {"empresa", "company"})},
                {"jobTitle", column(row, {"cargo", "jobTitle"})},
                {"source", column(row, {"fonte", "source"})},
                {"status", column(row, {"status"})},
                {"captureDate", column(row, {"data_captura", "captureDate"})},
                {"notes", column(row, {"observacoes", "notes"})}
            };

            if (payload["status"].get<std::string>().empty()) {
                payload["status"] = "novo";
            }
            if (payload["captureDate"].get<std::string>().empty()) {
                payload["captureDate"] = today_iso8601_utc();
            }

            std::string validationMessage;
            if (!validate_lead_payload(payload, validationMessage)) {
                errors.push_back("Linha " + std::to_string(line + 1) + ": " + validationMessage);
                continue;
            }

            auto lead = payload_to_lead(payload, auth->userId);
            std::string errorMessage;
            if (!firebase.create_lead(lead, errorMessage)) {
                errors.push_back("Linha " + std::to_string(line + 1) + ": " + errorMessage);
                continue;
            }
            ++imported;
        }

        send_json(res, 200, {
            {"imported", imported},
            {"errors", errors}
        });
    });

    std::cout << "Lead Manager rodando em http://localhost:" << config.port << std::endl;
    std::cout << "Colecoes Firestore: " << firebase.users_collection() << " / " << firebase.leads_collection() << std::endl;

    if (!server.listen("0.0.0.0", config.port)) {
        throw std::runtime_error("Nao foi possivel iniciar o servidor HTTP na porta " + std::to_string(config.port));
    }
}
