#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace csv_utils {

std::string export_leads_csv(const std::vector<nlohmann::json>& leads);
std::vector<nlohmann::json> import_leads_csv(const std::string& csv_content);

}
