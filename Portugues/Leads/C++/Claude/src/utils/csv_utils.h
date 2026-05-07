#pragma once
#include <string>
#include <vector>
#include <map>

namespace CsvUtils {

using Row    = std::map<std::string, std::string>;
using Table  = std::vector<Row>;

// Parse a CSV string (handles quoted fields, CRLF, LF)
Table parse(const std::string& csv);

// Generate a CSV string from headers + rows
std::string generate(const std::vector<std::string>& headers,
                     const std::vector<Row>& rows);

} // namespace CsvUtils
