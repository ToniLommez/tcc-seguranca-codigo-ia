#include "lead_manager/models.hpp"

namespace lead_manager {

json userToPublicJson(const User& user) {
  return {
      {"id", user.id},
      {"name", user.name},
      {"email", user.email},
      {"createdAt", user.createdAt},
  };
}

json interactionToJson(const Interaction& interaction) {
  return {
      {"timestamp", interaction.timestamp},
      {"type", interaction.type},
      {"notes", interaction.notes},
  };
}

json leadToJson(const Lead& lead) {
  json interactions = json::array();
  for (const auto& interaction : lead.interactions) {
    interactions.push_back(interactionToJson(interaction));
  }

  return {
      {"id", lead.id},
      {"userId", lead.userId},
      {"fullName", lead.fullName},
      {"email", lead.email},
      {"phone", lead.phone},
      {"company", lead.company},
      {"position", lead.position},
      {"source", lead.source},
      {"status", lead.status},
      {"captureDate", lead.captureDate},
      {"createdAt", lead.createdAt},
      {"updatedAt", lead.updatedAt},
      {"interactions", interactions},
  };
}

json leadListToJson(const LeadListResult& result) {
  json items = json::array();
  for (const auto& lead : result.items) {
    items.push_back(leadToJson(lead));
  }

  return {
      {"items", items},
      {"pagination",
       {
           {"total", result.total},
           {"page", result.page},
           {"pageSize", result.pageSize},
           {"totalPages", result.pageSize == 0 ? 0 : (result.total + result.pageSize - 1) / result.pageSize},
       }},
  };
}

}  // namespace lead_manager
