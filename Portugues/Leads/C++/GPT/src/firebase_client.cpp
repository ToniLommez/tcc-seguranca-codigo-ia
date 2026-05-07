#include "firebase_client.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

#include "http_client.hpp"
#include "utils.hpp"

namespace {

long long unix_now() {
    const auto now = std::chrono::system_clock::now();
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
}

AuthUser json_to_user(const json& document) {
    AuthUser user;
    user.id = document.value("id", "");
    user.name = document.value("name", "");
    user.email = document.value("email", "");
    user.passwordHash = document.value("passwordHash", "");
    user.createdAt = document.value("createdAt", "");
    user.updatedAt = document.value("updatedAt", "");
    return user;
}

json user_to_json(const AuthUser& user) {
    return {
        {"id", user.id},
        {"name", user.name},
        {"email", user.email},
        {"passwordHash", user.passwordHash},
        {"createdAt", user.createdAt},
        {"updatedAt", user.updatedAt}
    };
}

LeadRecord json_to_lead(const json& document) {
    LeadRecord lead;
    lead.id = document.value("id", "");
    lead.userId = document.value("userId", "");
    lead.fullName = document.value("fullName", "");
    lead.email = document.value("email", "");
    lead.phone = document.value("phone", "");
    lead.company = document.value("company", "");
    lead.jobTitle = document.value("jobTitle", "");
    lead.source = document.value("source", "");
    lead.status = document.value("status", "");
    lead.captureDate = document.value("captureDate", "");
    lead.notes = document.value("notes", "");
    lead.createdAt = document.value("createdAt", "");
    lead.updatedAt = document.value("updatedAt", "");
    lead.interactions = document.value("interactions", json::array());
    if (!lead.interactions.is_array()) {
        lead.interactions = json::array();
    }
    return lead;
}

json lead_to_json(const LeadRecord& lead) {
    return {
        {"id", lead.id},
        {"userId", lead.userId},
        {"fullName", lead.fullName},
        {"email", lead.email},
        {"phone", lead.phone},
        {"company", lead.company},
        {"jobTitle", lead.jobTitle},
        {"source", lead.source},
        {"status", lead.status},
        {"captureDate", lead.captureDate},
        {"notes", lead.notes},
        {"createdAt", lead.createdAt},
        {"updatedAt", lead.updatedAt},
        {"interactions", lead.interactions}
    };
}

}  // namespace

FirebaseClient::FirebaseClient(const AppConfig& config)
    : config_(config),
      credentials_(load_service_account(config.firebaseServiceAccountPath)),
      usersCollection_(config.collectionPrefix + "_users"),
      leadsCollection_(config.collectionPrefix + "_leads") {}

bool FirebaseClient::ping(std::string& errorMessage) {
    get_access_token(errorMessage);
    return errorMessage.empty();
}

bool FirebaseClient::create_user(const AuthUser& user, std::string& errorMessage) {
    return write_document(usersCollection_, user_document_id(user.email), user_to_json(user), true, errorMessage);
}

std::optional<AuthUser> FirebaseClient::get_user_by_email(const std::string& email, std::string& errorMessage) {
    const auto document = fetch_document(usersCollection_, user_document_id(email), errorMessage);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return json_to_user(*document);
}

bool FirebaseClient::create_lead(const LeadRecord& lead, std::string& errorMessage) {
    return write_document(leadsCollection_, lead.id, lead_to_json(lead), true, errorMessage);
}

std::optional<LeadRecord> FirebaseClient::get_lead(const std::string& leadId, std::string& errorMessage) {
    const auto document = fetch_document(leadsCollection_, leadId, errorMessage);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return json_to_lead(*document);
}

std::vector<LeadRecord> FirebaseClient::list_leads_for_user(const std::string& userId, std::string& errorMessage) {
    std::vector<LeadRecord> leads;
    std::string pageToken;

    for (;;) {
        auto url = firestore_base_url() + "/" + url_encode(leadsCollection_) + "?pageSize=200";
        if (!pageToken.empty()) {
            url += "&pageToken=" + url_encode(pageToken);
        }

        const auto token = get_access_token(errorMessage);
        if (!errorMessage.empty()) {
            return {};
        }

        const auto response = http_request(
            "GET",
            url,
            {
                {"Authorization", "Bearer " + token}
            }
        );

        if (response.status != 200) {
            errorMessage = "Falha ao listar leads no Firestore: HTTP " + std::to_string(response.status) + " - " + response.body;
            return {};
        }

        const auto payload = json::parse(response.body);
        if (payload.contains("documents") && payload["documents"].is_array()) {
            for (const auto& document : payload["documents"]) {
                const auto parsed = json_to_lead(from_firestore_document(document));
                if (parsed.userId == userId) {
                    leads.push_back(parsed);
                }
            }
        }

        pageToken = payload.value("nextPageToken", "");
        if (pageToken.empty()) {
            break;
        }
    }

    return leads;
}

bool FirebaseClient::update_lead(const LeadRecord& lead, std::string& errorMessage) {
    return write_document(leadsCollection_, lead.id, lead_to_json(lead), false, errorMessage);
}

bool FirebaseClient::delete_lead(const std::string& leadId, std::string& errorMessage) {
    const auto token = get_access_token(errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }

    const auto url = firestore_base_url() + "/" + url_encode(leadsCollection_) + "/" + url_encode(leadId);
    const auto response = http_request(
        "DELETE",
        url,
        {
            {"Authorization", "Bearer " + token}
        }
    );

    if (response.status == 200) {
        return true;
    }

    if (response.status == 404) {
        errorMessage = "Lead nao encontrado";
        return false;
    }

    errorMessage = "Falha ao excluir lead: HTTP " + std::to_string(response.status) + " - " + response.body;
    return false;
}

std::string FirebaseClient::users_collection() const {
    return usersCollection_;
}

std::string FirebaseClient::leads_collection() const {
    return leadsCollection_;
}

std::string FirebaseClient::get_access_token(std::string& errorMessage) {
    if (!accessToken_.empty() && accessTokenExpiresAt_ > unix_now() + 60) {
        return accessToken_;
    }

    errorMessage.clear();
    const auto assertion = sign_service_account_jwt(
        credentials_,
        credentials_.tokenUri,
        {
            {"scope", "https://www.googleapis.com/auth/datastore"}
        }
    );

    const auto response = http_request(
        "POST",
        credentials_.tokenUri,
        {
            {"Content-Type", "application/x-www-form-urlencoded"}
        },
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + url_encode(assertion)
    );

    if (response.status != 200) {
        errorMessage = "Falha ao obter access token do Google: HTTP " + std::to_string(response.status) + " - " + response.body;
        return {};
    }

    const auto payload = json::parse(response.body);
    accessToken_ = payload.value("access_token", "");
    accessTokenExpiresAt_ = unix_now() + payload.value("expires_in", 3600);

    if (accessToken_.empty()) {
        errorMessage = "Resposta de access token vazia";
    }
    return accessToken_;
}

std::string FirebaseClient::firestore_base_url() const {
    return "https://firestore.googleapis.com/v1/projects/" + credentials_.projectId + "/databases/(default)/documents";
}

json FirebaseClient::to_firestore_document(const json& source) const {
    json document;
    document["fields"] = json::object();
    for (const auto& [key, value] : source.items()) {
        document["fields"][key] = to_firestore_value(value);
    }
    return document;
}

json FirebaseClient::from_firestore_document(const json& document) const {
    json parsed = json::object();
    if (document.contains("name")) {
        const auto fullName = document["name"].get<std::string>();
        const auto separator = fullName.find_last_of('/');
        parsed["id"] = separator == std::string::npos ? fullName : fullName.substr(separator + 1);
    }

    if (document.contains("fields") && document["fields"].is_object()) {
        for (const auto& [key, value] : document["fields"].items()) {
            parsed[key] = from_firestore_value(value);
        }
    }
    return parsed;
}

json FirebaseClient::to_firestore_value(const json& value) const {
    if (value.is_null()) {
        return {{"nullValue", nullptr}};
    }
    if (value.is_boolean()) {
        return {{"booleanValue", value.get<bool>()}};
    }
    if (value.is_number_integer() || value.is_number_unsigned()) {
        return {{"integerValue", std::to_string(value.get<long long>())}};
    }
    if (value.is_number_float()) {
        return {{"doubleValue", value.get<double>()}};
    }
    if (value.is_string()) {
        return {{"stringValue", value.get<std::string>()}};
    }
    if (value.is_array()) {
        json values = json::array();
        for (const auto& item : value) {
            values.push_back(to_firestore_value(item));
        }
        return {{"arrayValue", {{"values", values}}}};
    }
    if (value.is_object()) {
        json fields = json::object();
        for (const auto& [key, item] : value.items()) {
            fields[key] = to_firestore_value(item);
        }
        return {{"mapValue", {{"fields", fields}}}};
    }
    return {{"stringValue", value.dump()}};
}

json FirebaseClient::from_firestore_value(const json& value) const {
    if (value.contains("nullValue")) {
        return nullptr;
    }
    if (value.contains("booleanValue")) {
        return value["booleanValue"].get<bool>();
    }
    if (value.contains("integerValue")) {
        return std::stoll(value["integerValue"].get<std::string>());
    }
    if (value.contains("doubleValue")) {
        return value["doubleValue"].get<double>();
    }
    if (value.contains("stringValue")) {
        return value["stringValue"].get<std::string>();
    }
    if (value.contains("arrayValue")) {
        json output = json::array();
        if (value["arrayValue"].contains("values")) {
            for (const auto& item : value["arrayValue"]["values"]) {
                output.push_back(from_firestore_value(item));
            }
        }
        return output;
    }
    if (value.contains("mapValue")) {
        json output = json::object();
        if (value["mapValue"].contains("fields")) {
            for (const auto& [key, item] : value["mapValue"]["fields"].items()) {
                output[key] = from_firestore_value(item);
            }
        }
        return output;
    }
    return nullptr;
}

std::optional<json> FirebaseClient::fetch_document(const std::string& collection, const std::string& documentId, std::string& errorMessage) {
    const auto token = get_access_token(errorMessage);
    if (!errorMessage.empty()) {
        return std::nullopt;
    }

    const auto response = http_request(
        "GET",
        firestore_base_url() + "/" + url_encode(collection) + "/" + url_encode(documentId),
        {
            {"Authorization", "Bearer " + token}
        }
    );

    if (response.status == 404) {
        return std::nullopt;
    }

    if (response.status != 200) {
        errorMessage = "Falha ao buscar documento no Firestore: HTTP " + std::to_string(response.status) + " - " + response.body;
        return std::nullopt;
    }

    return from_firestore_document(json::parse(response.body));
}

bool FirebaseClient::write_document(const std::string& collection, const std::string& documentId, const json& body, bool createOnly, std::string& errorMessage) {
    const auto token = get_access_token(errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }

    std::string method = "PATCH";
    std::string url = firestore_base_url() + "/" + url_encode(collection) + "/" + url_encode(documentId);
    if (createOnly) {
        method = "POST";
        url = firestore_base_url() + "/" + url_encode(collection) + "?documentId=" + url_encode(documentId);
    }

    const auto response = http_request(
        method,
        url,
        {
            {"Authorization", "Bearer " + token},
            {"Content-Type", "application/json"}
        },
        to_firestore_document(body).dump()
    );

    if (response.status == 200) {
        return true;
    }
    if (createOnly && response.status == 409) {
        errorMessage = "Documento ja existe";
        return false;
    }

    errorMessage = "Falha ao gravar documento no Firestore: HTTP " + std::to_string(response.status) + " - " + response.body;
    return false;
}
