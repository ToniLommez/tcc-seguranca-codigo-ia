#include "csv_utils.hpp"

#include <sstream>

namespace {

std::string escape_csv(const std::string& value) {
    const auto mustQuote = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!mustQuote) {
        return value;
    }

    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
}

}  // namespace

std::vector<std::vector<std::string>> parse_csv_rows(const std::string& content) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cell;
    bool insideQuotes = false;

    for (std::size_t i = 0; i < content.size(); ++i) {
        const char ch = content[i];
        if (insideQuotes) {
            if (ch == '"' && i + 1 < content.size() && content[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else if (ch == '"') {
                insideQuotes = false;
            } else {
                cell.push_back(ch);
            }
            continue;
        }

        if (ch == '"') {
            insideQuotes = true;
        } else if (ch == ',') {
            row.push_back(cell);
            cell.clear();
        } else if (ch == '\n') {
            row.push_back(cell);
            cell.clear();
            if (!(row.size() == 1 && row[0].empty())) {
                rows.push_back(row);
            }
            row.clear();
        } else if (ch != '\r') {
            cell.push_back(ch);
        }
    }

    row.push_back(cell);
    if (!(row.size() == 1 && row[0].empty())) {
        rows.push_back(row);
    }

    return rows;
}

std::string generate_csv(const std::vector<json>& leads) {
    std::ostringstream output;
    output << "id,nome_completo,email,telefone,empresa,cargo,fonte,status,data_captura,observacoes\n";
    for (const auto& lead : leads) {
        output
            << escape_csv(lead.value("id", "")) << ','
            << escape_csv(lead.value("fullName", "")) << ','
            << escape_csv(lead.value("email", "")) << ','
            << escape_csv(lead.value("phone", "")) << ','
            << escape_csv(lead.value("company", "")) << ','
            << escape_csv(lead.value("jobTitle", "")) << ','
            << escape_csv(lead.value("source", "")) << ','
            << escape_csv(lead.value("status", "")) << ','
            << escape_csv(lead.value("captureDate", "")) << ','
            << escape_csv(lead.value("notes", ""))
            << "\n";
    }
    return output.str();
}
