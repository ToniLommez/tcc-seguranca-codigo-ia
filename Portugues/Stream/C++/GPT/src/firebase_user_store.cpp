#include "firebase_user_store.h"

#include <chrono>
#include <fstream>
#include <stdexcept>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "utils.h"

namespace {
std::string firestore_string_field(const nlohmann::json& fields, const std::string& name) {
  if (!fields.contains(name) || !fields.at(name).contains("stringValue")) {
    return "";
  }
  return fields.at(name).at("stringValue").get<std::string>();
}
} // namespace

FirebaseUserStore::FirebaseUserStore(AppConfig config) : config_(std::move(config)) {
  load_service_account();
}

void FirebaseUserStore::load_service_account() {
  std::ifstream stream(config_.firebase_service_account_path);
  if (!stream) {
    throw std::runtime_error("Nao foi possivel abrir a service account do Firebase: " +
                             config_.firebase_service_account_path.string());
  }

  const auto root = nlohmann::json::parse(stream);
  service_account_.project_id = root.at("project_id").get<std::string>();
  service_account_.client_email = root.at("client_email").get<std::string>();
  service_account_.private_key = root.at("private_key").get<std::string>();
  service_account_.token_uri = root.at("token_uri").get<std::string>();
}

std::string FirebaseUserStore::document_id_for_email(const std::string& email) const {
  return utils::base64_url_encode(utils::to_lower(email));
}

std::string FirebaseUserStore::build_google_assertion() const {
  const auto now = utils::unix_now();
  nlohmann::json header = {
    {"alg", "RS256"},
    {"typ", "JWT"}
  };

  nlohmann::json payload = {
    {"iss", service_account_.client_email},
    {"scope", "https://www.googleapis.com/auth/datastore"},
    {"aud", service_account_.token_uri},
    {"iat", now},
    {"exp", now + 3600}
  };

  const std::string header_encoded = utils::base64_url_encode(header.dump());
  const std::string payload_encoded = utils::base64_url_encode(payload.dump());
  const std::string signing_input = header_encoded + "." + payload_encoded;
  const auto signature = utils::rsa_sha256_sign(service_account_.private_key, signing_input);

  return signing_input + "." + utils::base64_url_encode(signature);
}

std::string FirebaseUserStore::request_access_token() {
  const auto url = utils::parse_url(service_account_.token_uri);
  httplib::SSLClient client(url.host.c_str(), url.port);
  client.set_follow_location(true);
  client.set_connection_timeout(std::chrono::seconds(10));
  client.set_read_timeout(std::chrono::seconds(20));
  client.set_write_timeout(std::chrono::seconds(20));

  const std::string body =
    "grant_type=" +
    utils::url_encode("urn:ietf:params:oauth:grant-type:jwt-bearer") +
    "&assertion=" + utils::url_encode(build_google_assertion());

  auto response = client.Post(url.path.c_str(), body, "application/x-www-form-urlencoded");
  if (!response) {
    throw std::runtime_error("Falha ao solicitar token OAuth ao Google.");
  }

  if (response->status != 200) {
    throw std::runtime_error("Falha ao obter token OAuth do Google. HTTP " +
                             std::to_string(response->status) + ": " + response->body);
  }

  const auto json = nlohmann::json::parse(response->body);
  access_token_expires_at_ = utils::unix_now() + json.value("expires_in", 3600) - 60;
  return json.at("access_token").get<std::string>();
}

std::string FirebaseUserStore::ensure_access_token() {
  std::lock_guard<std::mutex> lock(token_mutex_);
  if (!access_token_.empty() && utils::unix_now() < access_token_expires_at_) {
    return access_token_;
  }

  access_token_ = request_access_token();
  return access_token_;
}

std::optional<nlohmann::json> FirebaseUserStore::fetch_document(const std::string& document_id) {
  httplib::SSLClient client("firestore.googleapis.com", 443);
  client.set_follow_location(true);
  client.set_connection_timeout(std::chrono::seconds(10));
  client.set_read_timeout(std::chrono::seconds(20));
  client.set_write_timeout(std::chrono::seconds(20));

  const std::string path = "/v1/projects/" + service_account_.project_id +
                           "/databases/(default)/documents/" + config_.firebase_collection + "/" +
                           document_id;

  httplib::Headers headers = {
    {"Authorization", "Bearer " + ensure_access_token()},
    {"Accept", "application/json"}
  };

  auto response = client.Get(path.c_str(), headers);
  if (!response) {
    throw std::runtime_error("Falha ao consultar usuario no Firestore.");
  }

  if (response->status == 404) {
    return std::nullopt;
  }

  if (response->status != 200) {
    throw std::runtime_error("Firestore retornou erro ao buscar usuario. HTTP " +
                             std::to_string(response->status) + ": " + response->body);
  }

  return nlohmann::json::parse(response->body);
}

bool FirebaseUserStore::create_document(const std::string& document_id,
                                        const nlohmann::json& document,
                                        std::string& error_message) {
  httplib::SSLClient client("firestore.googleapis.com", 443);
  client.set_follow_location(true);
  client.set_connection_timeout(std::chrono::seconds(10));
  client.set_read_timeout(std::chrono::seconds(20));
  client.set_write_timeout(std::chrono::seconds(20));

  const std::string path = "/v1/projects/" + service_account_.project_id +
                           "/databases/(default)/documents/" + config_.firebase_collection +
                           "?documentId=" + document_id;

  httplib::Headers headers = {
    {"Authorization", "Bearer " + ensure_access_token()},
    {"Accept", "application/json"}
  };

  auto response = client.Post(path.c_str(), headers, document.dump(), "application/json");
  if (!response) {
    error_message = "Falha de rede ao criar usuario no Firestore.";
    return false;
  }

  if (response->status == 409) {
    error_message = "Ja existe um usuario com este e-mail.";
    return false;
  }

  if (response->status != 200) {
    error_message = "Firestore retornou HTTP " + std::to_string(response->status) + ": " +
                    response->body;
    return false;
  }

  return true;
}

UserRecord FirebaseUserStore::document_to_user(const nlohmann::json& document) const {
  const auto& fields = document.at("fields");
  const auto type = user_type_from_string(firestore_string_field(fields, "type")).value_or(UserType::User);

  return UserRecord{
    firestore_string_field(fields, "name"),
    firestore_string_field(fields, "email"),
    firestore_string_field(fields, "passwordHash"),
    type,
    firestore_string_field(fields, "createdAt")
  };
}

std::optional<UserRecord> FirebaseUserStore::find_by_email(const std::string& email) {
  const auto document = fetch_document(document_id_for_email(email));
  if (!document.has_value()) {
    return std::nullopt;
  }

  return document_to_user(*document);
}

bool FirebaseUserStore::create_user(const UserRecord& user, std::string& error_message) {
  const nlohmann::json document = {
    {"fields",
     {
       {"name", {{"stringValue", user.name}}},
       {"email", {{"stringValue", utils::to_lower(user.email)}}},
       {"passwordHash", {{"stringValue", user.password_hash}}},
       {"type", {{"stringValue", user_type_to_string(user.type)}}},
       {"createdAt", {{"stringValue", user.created_at}}}
     }}
  };

  return create_document(document_id_for_email(user.email), document, error_message);
}
