#include "lead_manager/lead_service.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <sstream>

#include <tinyxml2.h>

#include "lead_manager/utils.hpp"

namespace lead_manager {

namespace {

const std::set<std::string> kAllowedStatuses = {"new", "contacted", "qualified", "lost"};

std::string stringOrDefault(const json& payload, const std::string& key, const std::string& fallback = "") {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return util::trim(payload.at(key).get<std::string>());
}

std::string normalizeSearchBlob(const Lead& lead) {
  return util::toLower(lead.fullName + " " + lead.email + " " + lead.company);
}

bool matchesFilters(const Lead& lead, const LeadQueryParams& params) {
  if (!params.status.empty() && lead.status != params.status) {
    return false;
  }
  if (!params.source.empty() && util::toLower(lead.source) != util::toLower(params.source)) {
    return false;
  }
  if (!params.captureDateFrom.empty() && lead.captureDate < params.captureDateFrom) {
    return false;
  }
  if (!params.captureDateTo.empty() && lead.captureDate > params.captureDateTo) {
    return false;
  }
  if (!params.search.empty()) {
    const std::string needle = util::toLower(params.search);
    if (lead.searchBlob.find(needle) == std::string::npos) {
      return false;
    }
  }
  return true;
}

bool compareLeads(const Lead& left, const Lead& right, const std::string& sortBy, const std::string& sortDirection) {
  auto applyDirection = [&](const std::string& a, const std::string& b) {
    if (sortDirection == "asc") {
      return a < b;
    }
    return a > b;
  };

  if (sortBy == "fullName") {
    return applyDirection(util::toLower(left.fullName), util::toLower(right.fullName));
  }
  if (sortBy == "company") {
    return applyDirection(util::toLower(left.company), util::toLower(right.company));
  }
  if (sortBy == "status") {
    return applyDirection(left.status, right.status);
  }
  if (sortBy == "source") {
    return applyDirection(util::toLower(left.source), util::toLower(right.source));
  }
  if (sortBy == "createdAt") {
    return applyDirection(left.createdAt, right.createdAt);
  }
  return applyDirection(left.captureDate, right.captureDate);
}

Lead makeLeadFromPayload(const std::string& userId,
                         const json& payload,
                         const std::optional<Lead>& existing,
                         std::string& error) {
  Lead lead = existing.value_or(Lead{});
  if (!existing.has_value()) {
    lead.id = util::generateId();
    lead.userId = userId;
    lead.createdAt = util::nowUtcIso();
  }

  if (payload.contains("fullName")) {
    lead.fullName = stringOrDefault(payload, "fullName");
  }
  if (payload.contains("email")) {
    lead.email = stringOrDefault(payload, "email");
  }
  if (payload.contains("phone")) {
    lead.phone = stringOrDefault(payload, "phone");
  }
  if (payload.contains("company")) {
    lead.company = stringOrDefault(payload, "company");
  }
  if (payload.contains("position")) {
    lead.position = stringOrDefault(payload, "position");
  }
  if (payload.contains("source")) {
    lead.source = stringOrDefault(payload, "source");
  }
  if (payload.contains("status")) {
    lead.status = util::toLower(stringOrDefault(payload, "status"));
  }
  if (payload.contains("captureDate")) {
    lead.captureDate = stringOrDefault(payload, "captureDate");
  }

  if (lead.fullName.empty()) {
    error = "Full name is required.";
    return {};
  }
  if (lead.email.empty() || lead.email.find('@') == std::string::npos) {
    error = "A valid email is required.";
    return {};
  }
  if (lead.source.empty()) {
    error = "Source is required.";
    return {};
  }
  if (lead.status.empty()) {
    lead.status = "new";
  }
  if (!kAllowedStatuses.contains(lead.status)) {
    error = "Status must be one of: new, contacted, qualified, lost.";
    return {};
  }
  if (lead.captureDate.empty()) {
    lead.captureDate = util::currentDateIso();
  }

  lead.updatedAt = util::nowUtcIso();
  lead.searchBlob = normalizeSearchBlob(lead);
  return lead;
}

std::map<std::string, std::size_t> buildHeaderIndex(const std::vector<std::string>& headers) {
  std::map<std::string, std::size_t> index;
  for (std::size_t position = 0; position < headers.size(); ++position) {
    const auto normalized = util::toLower(util::trim(headers[position]));
    index[normalized] = position;
  }
  return index;
}

std::string valueFromRow(const std::vector<std::string>& row,
                         const std::map<std::string, std::size_t>& headerIndex,
                         const std::vector<std::string>& names) {
  for (const auto& name : names) {
    const auto iterator = headerIndex.find(name);
    if (iterator != headerIndex.end() && iterator->second < row.size()) {
      return util::trim(row[iterator->second]);
    }
  }
  return "";
}

std::vector<std::vector<std::string>> parseSpreadsheetXml(const std::string& content) {
  tinyxml2::XMLDocument document;
  if (document.Parse(content.c_str()) != tinyxml2::XML_SUCCESS) {
    throw std::runtime_error("Unable to parse Excel XML file.");
  }

  std::vector<std::vector<std::string>> rows;
  auto* workbook = document.FirstChildElement("Workbook");
  if (workbook == nullptr) {
    throw std::runtime_error("Excel XML file does not contain a Workbook node.");
  }

  auto* worksheet = workbook->FirstChildElement("Worksheet");
  auto* table = worksheet == nullptr ? nullptr : worksheet->FirstChildElement("Table");
  for (auto* row = table == nullptr ? nullptr : table->FirstChildElement("Row");
       row != nullptr;
       row = row->NextSiblingElement("Row")) {
    std::vector<std::string> parsedRow;
    for (auto* cell = row->FirstChildElement("Cell"); cell != nullptr; cell = cell->NextSiblingElement("Cell")) {
      auto* data = cell->FirstChildElement("Data");
      parsedRow.push_back(data != nullptr && data->GetText() != nullptr ? util::xmlUnescape(data->GetText()) : "");
    }
    rows.push_back(parsedRow);
  }
  return rows;
}

}  // namespace

LeadService::LeadService(FirestoreClient& firestore) : firestore_(firestore) {}

LeadListResult LeadService::listLeads(const std::string& userId, const LeadQueryParams& params) {
  auto leads = firestore_.listLeadsForUser(userId);
  leads.erase(std::remove_if(leads.begin(),
                             leads.end(),
                             [&](const Lead& lead) {
                               return !matchesFilters(lead, params);
                             }),
              leads.end());

  std::sort(leads.begin(),
            leads.end(),
            [&](const Lead& left, const Lead& right) {
              return compareLeads(left, right, params.sortBy, params.sortDir);
            });

  LeadListResult result;
  result.total = static_cast<int>(leads.size());
  result.page = std::max(1, params.page);
  result.pageSize = std::max(1, params.pageSize);

  const int startIndex = (result.page - 1) * result.pageSize;
  if (startIndex < result.total) {
    const int endIndex = std::min(result.total, startIndex + result.pageSize);
    result.items.assign(leads.begin() + startIndex, leads.begin() + endIndex);
  }
  return result;
}

std::optional<Lead> LeadService::getLead(const std::string& userId, const std::string& leadId) {
  return firestore_.getLead(userId, leadId);
}

bool LeadService::createLead(const std::string& userId, const json& payload, Lead& lead, std::string& error) {
  lead = makeLeadFromPayload(userId, payload, std::nullopt, error);
  if (!error.empty()) {
    return false;
  }
  firestore_.upsertLead(lead);
  return true;
}

bool LeadService::updateLead(const std::string& userId,
                             const std::string& leadId,
                             const json& payload,
                             Lead& lead,
                             std::string& error) {
  const auto existing = firestore_.getLead(userId, leadId);
  if (!existing.has_value()) {
    error = "Lead not found.";
    return false;
  }

  lead = makeLeadFromPayload(userId, payload, existing, error);
  if (!error.empty()) {
    return false;
  }
  lead.id = leadId;
  lead.userId = userId;
  firestore_.upsertLead(lead);
  return true;
}

bool LeadService::deleteLead(const std::string& userId, const std::string& leadId) {
  return firestore_.deleteLead(userId, leadId);
}

bool LeadService::addInteraction(const std::string& userId,
                                 const std::string& leadId,
                                 const json& payload,
                                 Lead& lead,
                                 std::string& error) {
  auto existing = firestore_.getLead(userId, leadId);
  if (!existing.has_value()) {
    error = "Lead not found.";
    return false;
  }

  const std::string notes = stringOrDefault(payload, "notes");
  if (notes.empty()) {
    error = "Interaction notes are required.";
    return false;
  }

  Interaction interaction{
      .timestamp = util::nowUtcIso(),
      .type = stringOrDefault(payload, "type", "note"),
      .notes = notes,
  };

  lead = existing.value();
  lead.interactions.push_back(interaction);
  lead.updatedAt = util::nowUtcIso();
  firestore_.upsertLead(lead);
  return true;
}

json LeadService::dashboardSummary(const std::string& userId) {
  const auto leads = firestore_.listLeadsForUser(userId);

  std::map<std::string, int> byStatus;
  std::map<std::string, int> bySource;
  std::map<std::string, int> capturesOverTime;

  for (const auto& lead : leads) {
    byStatus[lead.status] += 1;
    bySource[lead.source] += 1;
    capturesOverTime[lead.captureDate] += 1;
  }

  json captures = json::array();
  for (const auto& [date, count] : capturesOverTime) {
    captures.push_back({{"date", date}, {"count", count}});
  }

  json statusItems = json::array();
  for (const auto& [status, count] : byStatus) {
    statusItems.push_back({{"label", status}, {"count", count}});
  }

  json sourceItems = json::array();
  for (const auto& [source, count] : bySource) {
    sourceItems.push_back({{"label", source}, {"count", count}});
  }

  return {
      {"totalLeads", static_cast<int>(leads.size())},
      {"statusSummary", statusItems},
      {"sourceSummary", sourceItems},
      {"capturesOverTime", captures},
  };
}

bool LeadService::importLeads(const std::string& userId,
                              const std::string& fileName,
                              const std::string& content,
                              int& importedCount,
                              std::string& error) {
  std::vector<std::vector<std::string>> rows;
  const std::string lowered = util::toLower(fileName);

  if (lowered.ends_with(".csv")) {
    rows = util::parseCsv(content);
  } else if (lowered.ends_with(".xls") || lowered.ends_with(".xml")) {
    rows = parseSpreadsheetXml(content);
  } else {
    error = "Unsupported import format. Use CSV or Excel XML (.xls).";
    return false;
  }

  if (rows.empty()) {
    error = "The import file is empty.";
    return false;
  }

  const auto headerIndex = buildHeaderIndex(rows.front());
  importedCount = 0;

  for (std::size_t rowIndex = 1; rowIndex < rows.size(); ++rowIndex) {
    const auto& row = rows[rowIndex];
    if (row.empty()) {
      continue;
    }

    json payload = {
        {"fullName", valueFromRow(row, headerIndex, {"full name", "fullname", "full_name", "name"})},
        {"email", valueFromRow(row, headerIndex, {"email"})},
        {"phone", valueFromRow(row, headerIndex, {"phone"})},
        {"company", valueFromRow(row, headerIndex, {"company"})},
        {"position", valueFromRow(row, headerIndex, {"position", "job title"})},
        {"source", valueFromRow(row, headerIndex, {"source"})},
        {"status", valueFromRow(row, headerIndex, {"status"})},
        {"captureDate", valueFromRow(row, headerIndex, {"capture date", "capture_date", "date"})},
    };

    if (stringOrDefault(payload, "fullName").empty() && stringOrDefault(payload, "email").empty()) {
      continue;
    }

    if (stringOrDefault(payload, "source").empty()) {
      payload["source"] = "import";
    }
    if (stringOrDefault(payload, "status").empty()) {
      payload["status"] = "new";
    }

    Lead lead;
    std::string rowError;
    if (createLead(userId, payload, lead, rowError)) {
      importedCount += 1;
    }
  }

  if (importedCount == 0) {
    error = "No valid leads were imported.";
    return false;
  }

  return true;
}

std::string LeadService::exportCsv(const std::string& userId) {
  const auto leads = firestore_.listLeadsForUser(userId);
  std::ostringstream stream;
  stream << "Full Name,Email,Phone,Company,Position,Source,Status,Capture Date\n";
  for (const auto& lead : leads) {
    stream << util::csvEscape(lead.fullName) << ','
           << util::csvEscape(lead.email) << ','
           << util::csvEscape(lead.phone) << ','
           << util::csvEscape(lead.company) << ','
           << util::csvEscape(lead.position) << ','
           << util::csvEscape(lead.source) << ','
           << util::csvEscape(lead.status) << ','
           << util::csvEscape(lead.captureDate) << '\n';
  }
  return stream.str();
}

std::string LeadService::exportExcelXml(const std::string& userId) {
  const auto leads = firestore_.listLeadsForUser(userId);
  std::ostringstream stream;
  stream << R"(<?xml version="1.0"?>)"
         << R"(<Workbook xmlns="urn:schemas-microsoft-com:office:spreadsheet" )"
         << R"(xmlns:ss="urn:schemas-microsoft-com:office:spreadsheet">)"
         << R"(<Worksheet ss:Name="Leads"><Table>)";

  const std::array<std::string, 8> headers = {
      "Full Name", "Email", "Phone", "Company", "Position", "Source", "Status", "Capture Date"};
  stream << "<Row>";
  for (const auto& header : headers) {
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(header) << "</Data></Cell>";
  }
  stream << "</Row>";

  for (const auto& lead : leads) {
    stream << "<Row>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.fullName) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.email) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.phone) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.company) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.position) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.source) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.status) << "</Data></Cell>";
    stream << "<Cell><Data ss:Type=\"String\">" << util::xmlEscape(lead.captureDate) << "</Data></Cell>";
    stream << "</Row>";
  }

  stream << "</Table></Worksheet></Workbook>";
  return stream.str();
}

}  // namespace lead_manager

