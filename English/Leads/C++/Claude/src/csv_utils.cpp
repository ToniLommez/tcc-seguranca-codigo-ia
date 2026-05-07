#include "csv_utils.h"
#include <sstream>
#include <algorithm>

namespace csv_utils {

static std::string escape_csv(const std::string& field) {
    if (field.find(',') != std::string::npos ||
        field.find('"') != std::string::npos ||
        field.find('\n') != std::string::npos) {
        std::string escaped = "\"";
        for (char c : field) {
            if (c == '"') escaped += "\"\"";
            else escaped += c;
        }
        escaped += "\"";
        return escaped;
    }
    return field;
}

static std::string get_str(const nlohmann::json& j, const std::string& key) {
    if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
    return "";
}

std::string export_leads_csv(const std::vector<nlohmann::json>& leads) {
    std::ostringstream oss;
    oss << "full_name,email,phone,company,position,source,status,capture_date,notes\r\n";

    for (auto& lead : leads) {
        oss << escape_csv(get_str(lead, "full_name")) << ","
            << escape_csv(get_str(lead, "email")) << ","
            << escape_csv(get_str(lead, "phone")) << ","
            << escape_csv(get_str(lead, "company")) << ","
            << escape_csv(get_str(lead, "position")) << ","
            << escape_csv(get_str(lead, "source")) << ","
            << escape_csv(get_str(lead, "status")) << ","
            << escape_csv(get_str(lead, "capture_date")) << ","
            << escape_csv(get_str(lead, "notes")) << "\r\n";
    }
    return oss.str();
}

static std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(field);
                field.clear();
            } else if (c != '\r') {
                field += c;
            }
        }
    }
    fields.push_back(field);
    return fields;
}

std::vector<nlohmann::json> import_leads_csv(const std::string& csv_content) {
    std::vector<nlohmann::json> leads;
    std::istringstream stream(csv_content);
    std::string line;

    std::getline(stream, line);
    auto headers = parse_csv_line(line);

    std::vector<std::string> expected = {
        "full_name", "email", "phone", "company", "position",
        "source", "status", "capture_date", "notes"
    };

    auto normalize = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
        return s;
    };

    std::vector<int> col_map(expected.size(), -1);
    for (size_t i = 0; i < expected.size(); ++i) {
        for (size_t j = 0; j < headers.size(); ++j) {
            if (normalize(headers[j]) == normalize(expected[i])) {
                col_map[i] = (int)j;
                break;
            }
        }
    }

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;
        auto fields = parse_csv_line(line);

        nlohmann::json lead;
        for (size_t i = 0; i < expected.size(); ++i) {
            if (col_map[i] >= 0 && col_map[i] < (int)fields.size()) {
                lead[expected[i]] = fields[col_map[i]];
            } else {
                lead[expected[i]] = "";
            }
        }

        if (lead["status"].get<std::string>().empty()) lead["status"] = "new";
        leads.push_back(lead);
    }

    return leads;
}

}
