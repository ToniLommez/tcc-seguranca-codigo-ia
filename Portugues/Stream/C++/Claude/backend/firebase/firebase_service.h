#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

struct UserDocument {
    std::string id;        // Firestore document ID (sanitized email)
    std::string name;
    std::string email;
    std::string passwordHash;
    std::string passwordSalt;
    std::string type;      // "ARTISTA" or "USUARIO"
};

class FirebaseService {
public:
    explicit FirebaseService(const std::string& credentialsPath);

    bool createUser(const UserDocument& user);
    std::optional<UserDocument> getUserByEmail(const std::string& email);

private:
    std::string getAccessToken();
    std::string firestorePost(const std::string& path,
                              const std::string& body,
                              const std::string& token);
    std::string firestoreGet(const std::string& path,
                             const std::string& token);

    static std::string emailToDocId(const std::string& email);
    static nlohmann::json userToFirestoreFields(const UserDocument& user);
    static UserDocument firestoreDocToUser(const nlohmann::json& doc, const std::string& docId);

    std::string m_clientEmail;
    std::string m_privateKey;
    std::string m_projectId;

    // Cached OAuth2 token
    std::string m_accessToken;
    long long   m_tokenExpiry = 0;
};
