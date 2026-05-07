#include "lead_controller.h"
#include "auth_controller.h"
#include "csv_utils.h"
#include <algorithm>
#include <sstream>

using json = nlohmann::json;

LeadController::LeadController(FirebaseClient& firebase) : firebase_(firebase) {}

std::string LeadController::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

void LeadController::create(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    try {
        auto body = json::parse(req.body);
        std::string now = get_timestamp();

        json lead_data = {
            {"user_id", user_id},
            {"full_name", body.value("full_name", "")},
            {"email", body.value("email", "")},
            {"phone", body.value("phone", "")},
            {"company", body.value("company", "")},
            {"position", body.value("position", "")},
            {"source", body.value("source", "website")},
            {"status", body.value("status", "new")},
            {"capture_date", body.value("capture_date", now.substr(0, 10))},
            {"notes", body.value("notes", "")},
            {"created_at", now},
            {"updated_at", now}
        };

        if (lead_data["full_name"].get<std::string>().empty()) {
            res.status = 400;
            res.set_content(json({{"error", "full_name is required"}}).dump(), "application/json");
            return;
        }

        auto doc_id = firebase_.create_document(COLLECTION, lead_data);
        if (!doc_id) {
            res.status = 500;
            res.set_content(json({{"error", "Failed to create lead"}}).dump(), "application/json");
            return;
        }

        lead_data["id"] = *doc_id;
        res.status = 201;
        res.set_content(lead_data.dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", std::string("Invalid request: ") + e.what()}}).dump(),
                       "application/json");
    }
}

void LeadController::get_all(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    int page = 1, page_size = 20;
    if (req.has_param("page")) page = std::max(1, std::stoi(req.get_param_value("page")));
    if (req.has_param("page_size")) page_size = std::clamp(std::stoi(req.get_param_value("page_size")), 1, 100);

    std::string sort_by = req.get_param_value("sort_by");
    std::string sort_order = req.get_param_value("sort_order");
    std::string filter_status = req.get_param_value("status");
    std::string filter_source = req.get_param_value("source");
    std::string filter_date_from = req.get_param_value("date_from");
    std::string filter_date_to = req.get_param_value("date_to");
    std::string search = req.get_param_value("search");

    if (sort_by.empty()) sort_by = "created_at";
    if (sort_order.empty()) sort_order = "desc";

    json query = {
        {"from", {{{"collectionId", COLLECTION}}}},
        {"where", {
            {"fieldFilter", {
                {"field", {{"fieldPath", "user_id"}}},
                {"op", "EQUAL"},
                {"value", {{"stringValue", user_id}}}
            }}
        }}
    };

    auto result = firebase_.run_query(COLLECTION, query);
    auto& docs = result.documents;

    auto match_filter = [&](const json& doc) -> bool {
        if (!filter_status.empty()) {
            if (doc.value("status", "") != filter_status) return false;
        }
        if (!filter_source.empty()) {
            if (doc.value("source", "") != filter_source) return false;
        }
        if (!filter_date_from.empty()) {
            if (doc.value("capture_date", "") < filter_date_from) return false;
        }
        if (!filter_date_to.empty()) {
            if (doc.value("capture_date", "") > filter_date_to) return false;
        }
        if (!search.empty()) {
            std::string s = search;
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            auto lower = [](std::string v) {
                std::transform(v.begin(), v.end(), v.begin(), ::tolower);
                return v;
            };
            bool found = lower(doc.value("full_name", "")).find(s) != std::string::npos ||
                         lower(doc.value("email", "")).find(s) != std::string::npos ||
                         lower(doc.value("company", "")).find(s) != std::string::npos ||
                         lower(doc.value("phone", "")).find(s) != std::string::npos;
            if (!found) return false;
        }
        return true;
    };

    std::vector<json> filtered;
    for (auto& doc : docs) {
        if (match_filter(doc)) filtered.push_back(doc);
    }

    std::sort(filtered.begin(), filtered.end(), [&](const json& a, const json& b) {
        std::string va = a.value(sort_by, "");
        std::string vb = b.value(sort_by, "");
        return sort_order == "asc" ? va < vb : va > vb;
    });

    int total = (int)filtered.size();
    int total_pages = (total + page_size - 1) / page_size;
    int offset = (page - 1) * page_size;

    std::vector<json> page_data;
    for (int i = offset; i < std::min(offset + page_size, total); ++i) {
        page_data.push_back(filtered[i]);
    }

    json response = {
        {"leads", page_data},
        {"pagination", {
            {"page", page},
            {"page_size", page_size},
            {"total", total},
            {"total_pages", total_pages}
        }}
    };
    res.set_content(response.dump(), "application/json");
}

void LeadController::get_one(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    std::string id = req.matches[1];
    auto doc = firebase_.get_document(COLLECTION, id);
    if (!doc || doc->value("user_id", "") != user_id) {
        res.status = 404;
        res.set_content(json({{"error", "Lead not found"}}).dump(), "application/json");
        return;
    }

    res.set_content(doc->dump(), "application/json");
}

void LeadController::update(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    std::string id = req.matches[1];
    auto existing = firebase_.get_document(COLLECTION, id);
    if (!existing || existing->value("user_id", "") != user_id) {
        res.status = 404;
        res.set_content(json({{"error", "Lead not found"}}).dump(), "application/json");
        return;
    }

    try {
        auto body = json::parse(req.body);
        json update_data;

        std::vector<std::string> allowed = {
            "full_name", "email", "phone", "company", "position",
            "source", "status", "capture_date", "notes"
        };
        for (auto& field : allowed) {
            if (body.contains(field)) {
                update_data[field] = body[field];
            }
        }
        update_data["updated_at"] = get_timestamp();

        if (!firebase_.update_document(COLLECTION, id, update_data)) {
            res.status = 500;
            res.set_content(json({{"error", "Failed to update lead"}}).dump(), "application/json");
            return;
        }

        auto updated = firebase_.get_document(COLLECTION, id);
        res.set_content(updated->dump(), "application/json");

    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", std::string("Invalid request: ") + e.what()}}).dump(),
                       "application/json");
    }
}

void LeadController::remove(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    std::string id = req.matches[1];
    auto existing = firebase_.get_document(COLLECTION, id);
    if (!existing || existing->value("user_id", "") != user_id) {
        res.status = 404;
        res.set_content(json({{"error", "Lead not found"}}).dump(), "application/json");
        return;
    }

    firebase_.delete_document(COLLECTION, id);
    res.set_content(json({{"message", "Lead deleted successfully"}}).dump(), "application/json");
}

void LeadController::export_csv(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    json query = {
        {"from", {{{"collectionId", COLLECTION}}}},
        {"where", {
            {"fieldFilter", {
                {"field", {{"fieldPath", "user_id"}}},
                {"op", "EQUAL"},
                {"value", {{"stringValue", user_id}}}
            }}
        }}
    };

    auto result = firebase_.run_query(COLLECTION, query);
    auto csv = csv_utils::export_leads_csv(result.documents);

    res.set_header("Content-Disposition", "attachment; filename=leads_export.csv");
    res.set_content(csv, "text/csv");
}

void LeadController::import_csv(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    std::string csv_content;
    if (req.has_file("file")) {
        auto& file = req.get_file_value("file");
        csv_content = file.content;
    } else {
        csv_content = req.body;
    }

    if (csv_content.empty()) {
        res.status = 400;
        res.set_content(json({{"error", "No CSV data provided"}}).dump(), "application/json");
        return;
    }

    auto leads = csv_utils::import_leads_csv(csv_content);
    std::string now = get_timestamp();
    int imported = 0;
    int failed = 0;

    for (auto& lead : leads) {
        lead["user_id"] = user_id;
        lead["created_at"] = now;
        lead["updated_at"] = now;
        if (lead.value("capture_date", "").empty()) {
            lead["capture_date"] = now.substr(0, 10);
        }

        auto doc_id = firebase_.create_document(COLLECTION, lead);
        if (doc_id) imported++;
        else failed++;
    }

    json response = {
        {"message", "Import completed"},
        {"imported", imported},
        {"failed", failed},
        {"total", (int)leads.size()}
    };
    res.set_content(response.dump(), "application/json");
}

void LeadController::dashboard(const httplib::Request& req, httplib::Response& res) {
    auto user_id = AuthController::extract_user_id(req);
    if (user_id.empty()) {
        res.status = 401;
        res.set_content(json({{"error", "Unauthorized"}}).dump(), "application/json");
        return;
    }

    json query = {
        {"from", {{{"collectionId", COLLECTION}}}},
        {"where", {
            {"fieldFilter", {
                {"field", {{"fieldPath", "user_id"}}},
                {"op", "EQUAL"},
                {"value", {{"stringValue", user_id}}}
            }}
        }}
    };

    auto result = firebase_.run_query(COLLECTION, query);

    std::map<std::string, int> by_status;
    std::map<std::string, int> by_source;
    std::map<std::string, int> by_date;

    for (auto& doc : result.documents) {
        std::string status = doc.value("status", "unknown");
        std::string source = doc.value("source", "unknown");
        std::string date = doc.value("capture_date", "unknown");

        by_status[status]++;
        by_source[source]++;
        if (date.length() >= 7) by_date[date.substr(0, 7)]++;
    }

    json response = {
        {"total_leads", (int)result.documents.size()},
        {"by_status", by_status},
        {"by_source", by_source},
        {"captures_over_time", by_date}
    };
    res.set_content(response.dump(), "application/json");
}
