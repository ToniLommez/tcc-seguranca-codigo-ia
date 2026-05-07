#include "csv_utils.h"
#include <sstream>

namespace CsvUtils {

// ---- Tokenise a single CSV line ----------------------------------------
static std::vector<std::string> parseLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                // Look-ahead: "" inside quotes is an escaped quote
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    fields.push_back(field);
    return fields;
}

// ---- Normalise line endings and split ----------------------------------
static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            lines.push_back(line);
            line.clear();
        } else if (text[i] == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += text[i];
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// ---- Escape a field value for CSV output -------------------------------
static std::string escapeField(const std::string& value) {
    bool needsQuoting = value.find_first_of(",\"\r\n") != std::string::npos;
    if (!needsQuoting) return value;

    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += '"';   // double-up quotes
        out += c;
    }
    out += '"';
    return out;
}

// ---- Public API --------------------------------------------------------

Table parse(const std::string& csv) {
    Table table;
    auto lines = splitLines(csv);
    if (lines.empty()) return table;

    // First line is the header
    auto headers = parseLine(lines[0]);

    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        auto fields = parseLine(lines[i]);
        Row row;
        for (size_t j = 0; j < headers.size(); ++j) {
            row[headers[j]] = (j < fields.size()) ? fields[j] : "";
        }
        table.push_back(row);
    }
    return table;
}

std::string generate(const std::vector<std::string>& headers,
                     const std::vector<Row>& rows) {
    std::ostringstream out;

    // BOM for Excel UTF-8 compatibility
    out << "\xEF\xBB\xBF";

    // Header line
    for (size_t i = 0; i < headers.size(); ++i) {
        if (i > 0) out << ',';
        out << escapeField(headers[i]);
    }
    out << "\r\n";

    // Data lines
    for (const auto& row : rows) {
        for (size_t i = 0; i < headers.size(); ++i) {
            if (i > 0) out << ',';
            auto it = row.find(headers[i]);
            out << escapeField(it != row.end() ? it->second : "");
        }
        out << "\r\n";
    }
    return out.str();
}

} // namespace CsvUtils
