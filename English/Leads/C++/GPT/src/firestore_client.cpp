#include "lead_manager/firestore_client.hpp"

#include <algorithm>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "lead_manager/utils.hpp"

namespace lead_manager {

namespace {

using json = nlohmann::json;

json stringField(const std::string& value) {
  return {{"stringValue", value}};
}

json boolField(bool value) {
  return {{"booleanValue", value}};
}

json arrayOfInteractions(const std::vector<Interaction>& interactions) {
  json values = json::array();
  for (const auto& interaction : interactions) {
    values.push_back({
        {"mapValue",
         {{"fields",
           {
               {"timestamp", stringField(interaction.timestamp)},
               {"type", stringField(interaction.type)},
               {"notes", stringField(interaction.notes)},
           }}}},
    });
  }
  return {{"arrayValue", {{"values", values}}}};
}

std::string fieldString(const json& fields, const std::string& name) {
  if (!fields.contains(name)) {
    return "";
  }
  const auto& field = fields.at(name);
  if (field.contains("stringValue")) {
    return field.at("stringValue").get<std::string>();
  }
  if (field.contains("integerValue")) {
    return field.at("integerValue").get<std::string>();
  }
  if (field.contains("booleanValue")) {
    return field.at("booleanValue").get<bool>() ? "true" : "false";
  }
  return "";
}

std::vector<Interaction> parseInteractions(const json& fields) {
  std::vector<Interaction> interactions;
  if (!fields.contains("interactions")) {
    return interactions;
  }

  const auto& interactionField = fields.at("interactions");
  if (!interactionField.contains("arrayValue")) {
    return interactions;
  }

  const auto& values = interactionField.at("arrayValue").value("values", json::array());
  for (const auto& item : values) {
    const auto& mapFields = item.at("mapValue").at("fields");
    interactions.push_back({
        .timestamp = fieldString(mapFields, "timestamp"),
        .type = fieldString(mapFields, "type"),
        .notes = fieldString(mapFields, "notes"),
    });
  }
  return interactions;
}

User userFromDocument(const json& document) {
  const auto& fields = document.at("fields");
  return {
      .id = fieldString(fields, "id"),
      .name = fieldString(fields, "name"),
      .email = fieldString(fields, "email"),
      .emailLower = fieldString(fields, "emailLower"),
      .passwordSalt = fieldString(fields, "passwordSalt"),
      .passwordHash = fieldString(fields, "passwordHash"),
      .createdAt = fieldString(fields, "createdAt"),
  };
}

Lead leadFromDocument(const json& document) {
  const auto& fields = document.at("fields");
  return {
      .id = fieldString(fields, "id"),
      .userId = fieldString(fields, "userId"),
      .fullName = fieldString(fields, "fullName"),
      .email = fieldString(fields, "email"),
      .phone = fieldString(fields, "phone"),
      .company = fieldString(fields, "company"),
      .position = fieldString(fields, "position"),
      .source = fieldString(fields, "source"),
      .status = fieldString(fields, "status"),
      .captureDate = fieldString(fields, "captureDate"),
      .createdAt = fieldString(fields, "createdAt"),
      .updatedAt = fieldString(fields, "updatedAt"),
      .searchBlob = fieldString(fields, "searchBlob"),
      .interactions = parseInteractions(fields),
  };
}

json userFields(const User& user) {
  return {
      {"id", stringField(user.id)},
      {"name", stringField(user.name)},
      {"email", stringField(user.email)},
      {"emailLower", stringField(user.emailLower)},
      {"passwordSalt", stringField(user.passwordSalt)},
      {"passwordHash", stringField(user.passwordHash)},
      {"createdAt", stringField(user.createdAt)},
  };
}

json leadFields(const Lead& lead) {
  return {
      {"id", stringField(lead.id)},
      {"userId", stringField(lead.userId)},
      {"fullName", stringField(lead.fullName)},
      {"email", stringField(lead.email)},
      {"phone", stringField(lead.phone)},
      {"company", stringField(lead.company)},
      {"position", stringField(lead.position)},
      {"source", stringField(lead.source)},
      {"status", stringField(lead.status)},
      {"captureDate", stringField(lead.captureDate)},
      {"createdAt", stringField(lead.createdAt)},
      {"updatedAt", stringField(lead.updatedAt)},
      {"searchBlob", stringField(lead.searchBlob)},
      {"interactions", arrayOfInteractions(lead.interactions)},
  };
}

json equalityQuery(const std::string& collection, const std::string& field, const std::string& value) {
  return {
      {"from", json::array({{{"collectionId", collection}}})},
      {"where",
       {{"fieldFilter",
         {
             {"field", {{"fieldPath", field}}},
             {"op", "EQUAL"},
             {"value", stringField(value)},
         }}}},
  };
}

}  // namespace

FirestoreClient::FirestoreClient(AppConfig config) : config_(std::move(config)) {
  const auto credentials = json::parse(util::readFile(config_.firebaseCredentialsPath));
  clientEmail_ = credentials.value("client_email", "");
  privateKey_ = credentials.value("private_key", "");
  if (clientEmail_.empty() || privateKey_.empty()) {
    throw std::runtime_error("Firebase credentials file is missing client_email or private_key.");
  }
}

std::string FirestoreClient::documentsBaseUrl() const {
  return "https://firestore.googleapis.com/v1/projects/" + config_.firestoreProjectId +
         "/databases/(default)/documents";
}

std::map<std::string, std::string> FirestoreClient::authHeaders() {
  return {
      {"Authorization", "Bearer " + getAccessToken()},
  };
}

std::string FirestoreClient::getAccessToken() {
  const auto now = std::time(nullptr);
  if (!accessToken_.empty() && now < accessTokenExpiry_) {
    return accessToken_;
  }

  const long long issuedAt = static_cast<long long>(now);
  const long long expiresAt = issuedAt + 3600;
  const json header = {{"alg", "RS256"}, {"typ", "JWT"}};
  const json payload = {
      {"iss", clientEmail_},
      {"scope", "https://www.googleapis.com/auth/datastore"},
      {"aud", "https://oauth2.googleapis.com/token"},
      {"iat", issuedAt},
      {"exp", expiresAt},
  };

  const std::string unsignedToken =
      util::base64UrlEncode(header.dump()) + "." + util::base64UrlEncode(payload.dump());
  const std::string signedToken = unsignedToken + "." + util::signRs256Base64Url(privateKey_, unsignedToken);

  const std::string body =
      "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + util::urlEncode(signedToken);

  const auto response = http_.request("POST",
                                      "https://oauth2.googleapis.com/token",
                                      {},
                                      body,
                                      "application/x-www-form-urlencoded");
  if (!response.ok()) {
    throw std::runtime_error("Failed to obtain Firebase access token: " + response.error + " " + response.body);
  }

  const auto tokenResponse = json::parse(response.body);
  accessToken_ = tokenResponse.value("access_token", "");
  const int expiresIn = tokenResponse.value("expires_in", 3600);
  accessTokenExpiry_ = now + std::max(60, expiresIn - 60);
  if (accessToken_.empty()) {
    throw std::runtime_error("Firebase token response did not contain an access_token.");
  }

  return accessToken_;
}

std::optional<FirestoreClient::json> FirestoreClient::getDocument(const std::string& collection,
                                                                  const std::string& documentId) {
  const auto response = http_.request("GET", documentsBaseUrl() + "/" + collection + "/" + documentId, authHeaders());
  if (response.status == 404) {
    return std::nullopt;
  }
  if (!response.ok()) {
    throw std::runtime_error("Failed to fetch Firestore document: " + response.error + " " + response.body);
  }
  return json::parse(response.body);
}

bool FirestoreClient::setDocument(const std::string& collection, const std::string& documentId, const json& fields) {
  const auto response = http_.request("PATCH",
                                      documentsBaseUrl() + "/" + collection + "/" + documentId,
                                      authHeaders(),
                                      json{{"fields", fields}}.dump());
  if (!response.ok()) {
    throw std::runtime_error("Failed to write Firestore document: " + response.error + " " + response.body);
  }
  return true;
}

bool FirestoreClient::deleteDocument(const std::string& collection, const std::string& documentId) {
  const auto response = http_.request("DELETE", documentsBaseUrl() + "/" + collection + "/" + documentId, authHeaders());
  if (response.status == 404) {
    return false;
  }
  if (!response.ok()) {
    throw std::runtime_error("Failed to delete Firestore document: " + response.error + " " + response.body);
  }
  return true;
}

FirestoreClient::json FirestoreClient::runQuery(const json& structuredQuery) {
  const auto response = http_.request("POST",
                                      documentsBaseUrl() + ":runQuery",
                                      authHeaders(),
                                      json{{"structuredQuery", structuredQuery}}.dump());
  if (!response.ok()) {
    throw std::runtime_error("Failed to run Firestore query: " + response.error + " " + response.body);
  }
  return json::parse(response.body);
}

std::optional<User> FirestoreClient::getUserByEmail(const std::string& email) {
  const auto results = runQuery(equalityQuery(config_.usersCollection, "emailLower", util::toLower(email)));
  for (const auto& item : results) {
    if (item.contains("document")) {
      return userFromDocument(item.at("document"));
    }
  }
  return std::nullopt;
}

std::optional<User> FirestoreClient::getUserById(const std::string& userId) {
  const auto document = getDocument(config_.usersCollection, userId);
  if (!document.has_value()) {
    return std::nullopt;
  }
  return userFromDocument(document.value());
}

bool FirestoreClient::upsertUser(const User& user) {
  return setDocument(config_.usersCollection, user.id, userFields(user));
}

std::vector<Lead> FirestoreClient::listLeadsForUser(const std::string& userId) {
  const auto results = runQuery(equalityQuery(config_.leadsCollection, "userId", userId));
  std::vector<Lead> leads;
  for (const auto& item : results) {
    if (item.contains("document")) {
      leads.push_back(leadFromDocument(item.at("document")));
    }
  }
  return leads;
}

std::optional<Lead> FirestoreClient::getLead(const std::string& userId, const std::string& leadId) {
  const auto document = getDocument(config_.leadsCollection, leadId);
  if (!document.has_value()) {
    return std::nullopt;
  }
  Lead lead = leadFromDocument(document.value());
  if (lead.userId != userId) {
    return std::nullopt;
  }
  return lead;
}

bool FirestoreClient::upsertLead(const Lead& lead) {
  return setDocument(config_.leadsCollection, lead.id, leadFields(lead));
}

bool FirestoreClient::deleteLead(const std::string& userId, const std::string& leadId) {
  const auto lead = getLead(userId, leadId);
  if (!lead.has_value()) {
    return false;
  }
  return deleteDocument(config_.leadsCollection, leadId);
}

}  // namespace lead_manager

