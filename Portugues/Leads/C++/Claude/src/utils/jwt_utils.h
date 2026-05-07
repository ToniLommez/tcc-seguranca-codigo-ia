#pragma once
#include <string>
#include <tuple>

namespace JwtUtils {

// ---- Application JWT (HS256) -------------------------------------------

// Creates a JWT for an authenticated user
std::string createToken(const std::string& userId,
                        const std::string& email,
                        const std::string& secret);

// Verifies JWT and extracts claims.
// Returns {valid, userId, email}
std::tuple<bool, std::string, std::string>
verifyToken(const std::string& token, const std::string& secret);

// ---- Google Service Account JWT (RS256) ---------------------------------

// Creates a short-lived JWT to exchange for a Google OAuth2 access token.
// pemPrivateKey must be a valid PKCS#8 PEM private key.
std::string createGoogleJwt(const std::string& serviceAccountEmail,
                             const std::string& pemPrivateKey,
                             const std::string& privateKeyId);

} // namespace JwtUtils
