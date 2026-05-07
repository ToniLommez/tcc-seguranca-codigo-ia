#pragma once

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace lead_manager {

using json = nlohmann::json;

struct User {
  std::string id;
  std::string name;
  std::string email;
  std::string emailLower;
  std::string passwordSalt;
  std::string passwordHash;
  std::string createdAt;
};

struct Interaction {
  std::string timestamp;
  std::string type;
  std::string notes;
};

struct Lead {
  std::string id;
  std::string userId;
  std::string fullName;
  std::string email;
  std::string phone;
  std::string company;
  std::string position;
  std::string source;
  std::string status;
  std::string captureDate;
  std::string createdAt;
  std::string updatedAt;
  std::string searchBlob;
  std::vector<Interaction> interactions;
};

struct AuthenticatedUser {
  std::string id;
  std::string email;
  std::string name;
};

struct LeadQueryParams {
  int page = 1;
  int pageSize = 10;
  std::string sortBy = "captureDate";
  std::string sortDir = "desc";
  std::string status;
  std::string source;
  std::string captureDateFrom;
  std::string captureDateTo;
  std::string search;
};

struct LeadListResult {
  std::vector<Lead> items;
  int total = 0;
  int page = 1;
  int pageSize = 10;
};

json userToPublicJson(const User& user);
json interactionToJson(const Interaction& interaction);
json leadToJson(const Lead& lead);
json leadListToJson(const LeadListResult& result);

}  // namespace lead_manager

