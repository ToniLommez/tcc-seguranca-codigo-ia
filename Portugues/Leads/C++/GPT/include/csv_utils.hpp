#pragma once

#include <string>
#include <vector>

#include "../external/json.hpp"

using json = nlohmann::json;

std::vector<std::vector<std::string>> parse_csv_rows(const std::string& content);
std::string generate_csv(const std::vector<json>& leads);

