#include "leads_routes.h"
#include "../config.h"
#include "../utils/hash_utils.h"
#include "../utils/jwt_utils.h"
#include "../utils/csv_utils.h"
#include <nlohmann/json.hpp>
#include <crow.h>
#include <algorithm>
#include <chrono>
#include <sstream>

using json = nlohmann::json;

// ---- Helpers -----------------------------------------------------------

static std::tuple<bool, std::string, std::string>
requireAuth(const crow::request& req) {
    auto auth = req.get_header_value("Authorization");
    if (auth.size() < 8 || auth.substr(0, 7) != "Bearer ")
        return {false, "", ""};
    return JwtUtils::verifyToken(auth.substr(7), JWT_SECRET);
}

static crow::response jsonResp(int code, const json& body) {
    crow::response res(code, body.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

static std::string nowIso() {
    using namespace std::chrono;
    auto t = system_clock::now();
    auto tt = system_clock::to_time_t(t);
    struct tm tmBuf;
#ifdef _WIN32
    gmtime_s(&tmBuf, &tt);
#else
    gmtime_r(&tt, &tmBuf);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return buf;
}

static bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// Validate lead fields
static std::string validateLead(const json& body) {
    if (!body.contains("full_name") || body["full_name"].get<std::string>().empty())
        return "full_name e obrigatorio";
    if (!body.contains("email") || body["email"].get<std::string>().empty())
        return "email e obrigatorio";
    return "";
}

// Build a lead JSON from request body (merging with defaults)
static json buildLeadDoc(const json& body, const std::string& userId,
                          bool isNew = true) {
    json doc;
    doc["id"]           = isNew ? HashUtils::generateUUID()
                                : body.value("id", "");
    doc["user_id"]      = userId;
    doc["full_name"]    = body.value("full_name",    "");
    doc["email"]        = body.value("email",        "");
    doc["phone"]        = body.value("phone",        "");
    doc["company"]      = body.value("company",      "");
    doc["position"]     = body.value("position",     "");
    doc["source"]       = body.value("source",       "site");
    doc["status"]       = body.value("status",       "new");
    doc["capture_date"] = body.value("capture_date", nowIso().substr(0, 10));
    doc["notes"]        = body.value("notes",        "");
    doc["interactions"] = body.value("interactions", json::array());
    if (isNew) doc["created_at"] = nowIso();
    doc["updated_at"] = nowIso();
    return doc;
}

// CSV column names / lead field mapping
static const std::vector<std::string> CSV_HEADERS = {
    "id","full_name","email","phone","company","position",
    "source","status","capture_date","notes","created_at"
};

static CsvUtils::Row leadToCsvRow(const json& lead) {
    CsvUtils::Row row;
    for (auto& h : CSV_HEADERS)
        row[h] = lead.value(h, "");
    return row;
}

// ---- Route setup -------------------------------------------------------
void setupLeadsRoutes(crow::SimpleApp& app,
                      std::shared_ptr<FirebaseClient> firebase) {

    // GET /api/leads  – list with pagination, filters, sorting
    CROW_ROUTE(app, "/api/leads")
    .methods("GET"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        // Query params
        auto statusParam  = req.url_params.get("status");
        auto sourceParam  = req.url_params.get("source");
        auto searchParam  = req.url_params.get("search");
        auto dateFromParam= req.url_params.get("date_from");
        auto dateToParam  = req.url_params.get("date_to");
        auto sortBy       = req.url_params.get("sort_by");
        auto sortDir      = req.url_params.get("sort_dir");
        auto pageStr      = req.url_params.get("page");
        auto limitStr     = req.url_params.get("limit");

        int page  = pageStr  ? std::max(1, std::stoi(pageStr))  : 1;
        int limit = limitStr ? std::min(MAX_PAGE_SIZE, std::stoi(limitStr))
                             : DEFAULT_PAGE_SIZE;

        std::string sortField = sortBy  ? std::string(sortBy)  : "created_at";
        std::string sortOrder = sortDir ? std::string(sortDir) : "desc";

        try {
            // Build Firestore query filters
            json filters = json::array();

            // Always filter by user_id
            filters.push_back({
                {"fieldFilter", {
                    {"field", {{"fieldPath", "user_id"}}},
                    {"op",    "EQUAL"},
                    {"value", {{"stringValue", userId}}}
                }}
            });

            if (statusParam)
                filters.push_back({
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "status"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", std::string(statusParam)}}}
                    }}
                });

            if (sourceParam)
                filters.push_back({
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "source"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", std::string(sourceParam)}}}
                    }}
                });

            if (dateFromParam)
                filters.push_back({
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "capture_date"}}},
                        {"op",    "GREATER_THAN_OR_EQUAL"},
                        {"value", {{"stringValue", std::string(dateFromParam)}}}
                    }}
                });

            if (dateToParam)
                filters.push_back({
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "capture_date"}}},
                        {"op",    "LESS_THAN_OR_EQUAL"},
                        {"value", {{"stringValue", std::string(dateToParam)}}}
                    }}
                });

            json whereClause;
            if (filters.size() == 1) {
                whereClause = filters[0];
            } else {
                whereClause = {
                    {"compositeFilter", {
                        {"op",      "AND"},
                        {"filters", filters}
                    }}
                };
            }

            std::string direction = (sortOrder == "asc") ? "ASCENDING" : "DESCENDING";
            json query = {
                {"from",    json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where",   whereClause},
                {"orderBy", json::array({{
                    {"field",     {{"fieldPath", sortField}}},
                    {"direction", direction}
                }})},
                {"limit",  limit * page + limit}  // fetch a bit more for pagination
            };

            auto allLeads = firebase->runQuery(query);

            // In-memory text search (Firestore lacks full-text search)
            if (searchParam) {
                std::string search(searchParam);
                allLeads.erase(
                    std::remove_if(allLeads.begin(), allLeads.end(),
                        [&](const json& lead) {
                            return !containsCI(lead.value("full_name",""), search) &&
                                   !containsCI(lead.value("email",    ""), search) &&
                                   !containsCI(lead.value("company",  ""), search);
                        }),
                    allLeads.end());
            }

            int total  = (int)allLeads.size();
            int offset = (page - 1) * limit;
            json paginated = json::array();
            for (int i = offset; i < std::min((int)allLeads.size(), offset + limit); ++i)
                paginated.push_back(allLeads[i]);

            return jsonResp(200, {
                {"data",       paginated},
                {"total",      total},
                {"page",       page},
                {"limit",      limit},
                {"totalPages", (total + limit - 1) / limit}
            });
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // POST /api/leads  – create
    CROW_ROUTE(app, "/api/leads")
    .methods("POST"_method)
    ([firebase](const crow::request& req) -> crow::response {
        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        json body;
        try { body = json::parse(req.body); }
        catch (...) { return jsonResp(400, {{"error", "JSON invalido"}}); }

        std::string err = validateLead(body);
        if (!err.empty()) return jsonResp(400, {{"error", err}});

        try {
            json lead = buildLeadDoc(body, userId, true);
            firebase->createDocument(COLLECTION_LEADS, lead);
            return jsonResp(201, lead);
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // GET /api/leads/export  – export CSV (must come BEFORE /:id)
    CROW_ROUTE(app, "/api/leads/export")
    .methods("GET"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        try {
            json query = {
                {"from",  json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where", {
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "user_id"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", userId}}}
                    }}
                }},
                {"orderBy", json::array({{
                    {"field",     {{"fieldPath", "created_at"}}},
                    {"direction", "DESCENDING"}
                }})},
                {"limit", 10000}
            };
            auto leads = firebase->runQuery(query);

            std::vector<CsvUtils::Row> rows;
            for (auto& lead : leads) rows.push_back(leadToCsvRow(lead));

            std::string csv = CsvUtils::generate(CSV_HEADERS, rows);
            crow::response res(200, csv);
            res.add_header("Content-Type", "text/csv; charset=utf-8");
            res.add_header("Content-Disposition",
                           "attachment; filename=\"leads.csv\"");
            return res;
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // POST /api/leads/import  – import CSV
    CROW_ROUTE(app, "/api/leads/import")
    .methods("POST"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        std::string csvContent;

        // Detect multipart vs. plain body
        auto ct = req.get_header_value("Content-Type");
        if (ct.find("multipart/form-data") != std::string::npos) {
            crow::multipart::message mp(req);
            for (auto& part : mp.parts) {
                // Check Content-Disposition for name="file"
                bool isFile = false;
                for (auto& hdr : part.headers) {
                    if (hdr.second.value.find("name=\"file\"") != std::string::npos ||
                        hdr.second.value.find("name=file") != std::string::npos) {
                        isFile = true;
                        break;
                    }
                }
                if (isFile || mp.parts.size() == 1) {
                    csvContent = part.body;
                    break;
                }
            }
        } else {
            csvContent = req.body;  // plain CSV body
        }

        if (csvContent.empty())
            return jsonResp(400, {{"error", "Arquivo CSV vazio ou ausente"}});

        try {
            auto table = CsvUtils::parse(csvContent);
            int imported = 0, errors = 0;
            json errorList = json::array();

            for (size_t i = 0; i < table.size(); ++i) {
                auto& row = table[i];
                json body;
                for (auto& [k, v] : row) body[k] = v;
                body["user_id"] = userId;

                std::string err = validateLead(body);
                if (!err.empty()) {
                    errors++;
                    errorList.push_back({
                        {"row",   (int)(i + 2)},
                        {"error", err}
                    });
                    continue;
                }

                json lead = buildLeadDoc(body, userId, true);
                firebase->createDocument(COLLECTION_LEADS, lead);
                imported++;
            }

            return jsonResp(200, {
                {"imported", imported},
                {"errors",   errors},
                {"errorList",errorList}
            });
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // GET /api/leads/:id  – get single lead
    CROW_ROUTE(app, "/api/leads/<string>")
    .methods("GET"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req, const std::string& leadId) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        if (leadId == "export" || leadId == "import")
            return jsonResp(404, {{"error", "Not found"}});

        try {
            // Find lead by id field (not Firestore document name)
            json query = {
                {"from",  json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where", {
                    {"compositeFilter", {
                        {"op", "AND"},
                        {"filters", json::array({
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", leadId}}}
                            }}},
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "user_id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", userId}}}
                            }}}
                        })}
                    }}
                }},
                {"limit", 1}
            };
            auto results = firebase->runQuery(query);
            if (results.empty())
                return jsonResp(404, {{"error", "Lead nao encontrado"}});

            return jsonResp(200, results[0]);
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // PUT /api/leads/:id  – update lead
    CROW_ROUTE(app, "/api/leads/<string>")
    .methods("PUT"_method)
    ([firebase](const crow::request& req, const std::string& leadId) -> crow::response {
        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        json body;
        try { body = json::parse(req.body); }
        catch (...) { return jsonResp(400, {{"error", "JSON invalido"}}); }

        try {
            // Find the Firestore document name
            json q = {
                {"from",  json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where", {
                    {"compositeFilter", {
                        {"op", "AND"},
                        {"filters", json::array({
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", leadId}}}
                            }}},
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "user_id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", userId}}}
                            }}}
                        })}
                    }}
                }},
                {"limit", 1}
            };
            auto results = firebase->runQuery(q);
            if (results.empty())
                return jsonResp(404, {{"error", "Lead nao encontrado"}});

            auto& existing = results[0];
            std::string docId = existing.value("id", "");

            // Merge existing with updates
            json updated = existing;
            for (auto& [k, v] : body.items()) updated[k] = v;
            updated["updated_at"] = nowIso();

            firebase->updateDocument(COLLECTION_LEADS, docId, updated);
            return jsonResp(200, updated);
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // DELETE /api/leads/:id
    CROW_ROUTE(app, "/api/leads/<string>")
    .methods("DELETE"_method)
    ([firebase](const crow::request& req, const std::string& leadId) -> crow::response {
        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        try {
            json q = {
                {"from",  json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where", {
                    {"compositeFilter", {
                        {"op", "AND"},
                        {"filters", json::array({
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", leadId}}}
                            }}},
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "user_id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", userId}}}
                            }}}
                        })}
                    }}
                }},
                {"limit", 1}
            };
            auto results = firebase->runQuery(q);
            if (results.empty())
                return jsonResp(404, {{"error", "Lead nao encontrado"}});

            std::string docId = results[0].value("id", "");
            firebase->deleteDocument(COLLECTION_LEADS, docId);
            return jsonResp(200, {{"message", "Lead excluido com sucesso"}});
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // POST /api/leads/:id/interactions  – add interaction
    CROW_ROUTE(app, "/api/leads/<string>/interactions")
    .methods("POST"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req, const std::string& leadId) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        json body;
        try { body = json::parse(req.body); }
        catch (...) { return jsonResp(400, {{"error", "JSON invalido"}}); }

        if (!body.contains("description") ||
            body["description"].get<std::string>().empty())
            return jsonResp(400, {{"error", "description e obrigatoria"}});

        try {
            json q = {
                {"from",  json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where", {
                    {"compositeFilter", {
                        {"op", "AND"},
                        {"filters", json::array({
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", leadId}}}
                            }}},
                            {{"fieldFilter", {
                                {"field", {{"fieldPath", "user_id"}}},
                                {"op",    "EQUAL"},
                                {"value", {{"stringValue", userId}}}
                            }}}
                        })}
                    }}
                }},
                {"limit", 1}
            };
            auto results = firebase->runQuery(q);
            if (results.empty())
                return jsonResp(404, {{"error", "Lead nao encontrado"}});

            auto& lead = results[0];
            json interactions = lead.value("interactions", json::array());

            json newInteraction = {
                {"id",          HashUtils::generateUUID()},
                {"type",        body.value("type",        "note")},
                {"description", body.value("description", "")},
                {"date",        nowIso()},
                {"user_id",     userId}
            };
            interactions.push_back(newInteraction);

            json updates = {
                {"interactions", interactions},
                {"updated_at",   nowIso()}
            };
            std::string docId = lead.value("id", "");
            firebase->updateDocument(COLLECTION_LEADS, docId, updates);

            lead["interactions"] = interactions;
            lead["updated_at"]   = updates["updated_at"];
            return jsonResp(201, lead);
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });

    // GET /api/leads/stats  – dashboard stats
    CROW_ROUTE(app, "/api/leads/stats")
    .methods("GET"_method, "OPTIONS"_method)
    ([firebase](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::Options)
            return crow::response(204);

        auto [valid, userId, userEmail] = requireAuth(req);
        if (!valid) return jsonResp(401, {{"error", "Nao autorizado"}});

        try {
            json q = {
                {"from",  json::array({{{"collectionId", COLLECTION_LEADS}}})},
                {"where", {
                    {"fieldFilter", {
                        {"field", {{"fieldPath", "user_id"}}},
                        {"op",    "EQUAL"},
                        {"value", {{"stringValue", userId}}}
                    }}
                }},
                {"limit", 10000}
            };
            auto leads = firebase->runQuery(q);

            // Count by status
            std::map<std::string, int> byStatus = {
                {"new", 0}, {"in_contact", 0}, {"qualified", 0}, {"lost", 0}
            };
            std::map<std::string, int> bySource;
            std::map<std::string, int> byMonth;

            for (auto& lead : leads) {
                std::string status = lead.value("status", "new");
                byStatus[status]++;

                std::string source = lead.value("source", "other");
                bySource[source]++;

                std::string cd = lead.value("capture_date", "");
                if (cd.size() >= 7) {
                    std::string month = cd.substr(0, 7);  // YYYY-MM
                    byMonth[month]++;
                }
            }

            // Last 6 months for chart
            json monthlyData = json::array();
            for (auto& [m, c] : byMonth) {
                monthlyData.push_back({{"month", m}, {"count", c}});
            }

            return jsonResp(200, {
                {"total",       (int)leads.size()},
                {"by_status",   byStatus},
                {"by_source",   bySource},
                {"monthly",     monthlyData}
            });
        } catch (const std::exception& e) {
            return jsonResp(500, {{"error", e.what()}});
        }
    });
}
