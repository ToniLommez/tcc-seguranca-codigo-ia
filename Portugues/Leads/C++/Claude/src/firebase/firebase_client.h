#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

// FirebaseClient wraps the Firestore REST API and handles OAuth2
// token refresh automatically.
class FirebaseClient {
public:
    explicit FirebaseClient(const std::string& credentialsPath);

    // ---- Low-level Firestore operations --------------------------------

    // Create a new document, returns the assigned document ID
    std::string createDocument(const std::string& collection,
                               const nlohmann::json& fields);

    // Retrieve a document by ID
    std::optional<nlohmann::json> getDocument(const std::string& collection,
                                              const std::string& docId);

    // Update fields in an existing document
    bool updateDocument(const std::string& collection,
                        const std::string& docId,
                        const nlohmann::json& fields);

    // Delete a document
    bool deleteDocument(const std::string& collection,
                        const std::string& docId);

    // Run a structured query; returns an array of {id, fields} objects
    nlohmann::json runQuery(const nlohmann::json& structuredQuery);

    // List documents (simple, no filtering)
    nlohmann::json listDocuments(const std::string& collection,
                                 int pageSize  = 20,
                                 const std::string& pageToken = "");

    // ---- Higher-level helpers ------------------------------------------

    // Convert a flat JSON object -> Firestore "fields" map
    static nlohmann::json toFirestoreFields(const nlohmann::json& obj);

    // Convert a Firestore "fields" map -> flat JSON object
    static nlohmann::json fromFirestoreFields(const nlohmann::json& fields);

    // Extract the document ID from a Firestore resource name
    static std::string extractDocId(const std::string& name);

private:
    std::string projectId_;
    std::string serviceAccountEmail_;
    std::string privateKey_;
    std::string privateKeyId_;

    std::string accessToken_;
    long long   tokenExpiry_ = 0;   // epoch seconds

    const std::string& getAccessToken();
    void refreshAccessToken();

    // HTTP helper (returns {statusCode, body})
    std::pair<int, std::string> httpGet(const std::string& url);
    std::pair<int, std::string> httpPost(const std::string& url,
                                         const std::string& body,
                                         const std::string& contentType = "application/json");
    std::pair<int, std::string> httpPatch(const std::string& url,
                                          const std::string& body);
    std::pair<int, std::string> httpDelete(const std::string& url);

    std::string firestoreBase() const;
};
