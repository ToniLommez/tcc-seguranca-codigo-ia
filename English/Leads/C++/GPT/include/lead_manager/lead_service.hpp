#pragma once

#include <optional>
#include <string>

#include "lead_manager/firestore_client.hpp"
#include "lead_manager/models.hpp"

namespace lead_manager {

class LeadService {
 public:
  explicit LeadService(FirestoreClient& firestore);

  LeadListResult listLeads(const std::string& userId, const LeadQueryParams& params);
  std::optional<Lead> getLead(const std::string& userId, const std::string& leadId);
  bool createLead(const std::string& userId,
                  const json& payload,
                  Lead& lead,
                  std::string& error);
  bool updateLead(const std::string& userId,
                  const std::string& leadId,
                  const json& payload,
                  Lead& lead,
                  std::string& error);
  bool deleteLead(const std::string& userId, const std::string& leadId);
  bool addInteraction(const std::string& userId,
                      const std::string& leadId,
                      const json& payload,
                      Lead& lead,
                      std::string& error);
  json dashboardSummary(const std::string& userId);
  bool importLeads(const std::string& userId,
                   const std::string& fileName,
                   const std::string& content,
                   int& importedCount,
                   std::string& error);
  std::string exportCsv(const std::string& userId);
  std::string exportExcelXml(const std::string& userId);

 private:
  FirestoreClient& firestore_;
};

}  // namespace lead_manager
