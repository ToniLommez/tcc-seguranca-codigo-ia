#include "jwt_helper.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <cstring>

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------

static std::string rawBase64Encode(const unsigned char* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, mem);
    BIO_write(b64, data, (int)len);
    BIO_flush(b64);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

static std::vector<unsigned char> rawBase64Decode(const std::string& b64) {
    BIO* b64bio = BIO_new(BIO_f_base64());
    BIO* mem    = BIO_new_mem_buf(b64.data(), (int)b64.size());
    BIO_set_flags(b64bio, BIO_FLAGS_BASE64_NO_NL);
    mem = BIO_push(b64bio, mem);
    std::vector<unsigned char> buf(b64.size());
    int n = BIO_read(mem, buf.data(), (int)buf.size());
    BIO_free_all(mem);
    if (n < 0) n = 0;
    buf.resize(n);
    return buf;
}

// --------------------------------------------------------------------------
// Public helpers
// --------------------------------------------------------------------------

std::string JwtHelper::base64UrlEncode(const unsigned char* data, size_t len) {
    std::string s = rawBase64Encode(data, len);
    for (auto& c : s) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!s.empty() && s.back() == '=') s.pop_back();
    return s;
}

std::string JwtHelper::base64UrlEncodeStr(const std::string& s) {
    return base64UrlEncode(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

std::vector<unsigned char> JwtHelper::base64UrlDecode(const std::string& input) {
    std::string b64 = input;
    for (auto& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (b64.size() % 4 != 0) b64 += '=';
    return rawBase64Decode(b64);
}

// --------------------------------------------------------------------------
// HMAC-SHA256
// --------------------------------------------------------------------------

static std::vector<unsigned char> hmacSha256(const std::string& data, const std::string& key) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  dlen = 0;
    HMAC(EVP_sha256(),
         key.data(), (int)key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &dlen);
    return std::vector<unsigned char>(digest, digest + dlen);
}

// --------------------------------------------------------------------------
// HS256 token creation / validation
// --------------------------------------------------------------------------

std::string JwtHelper::createToken(const std::string& userId,
                                   const std::string& email,
                                   const std::string& userType,
                                   const std::string& secret) {
    nlohmann::json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    nlohmann::json payload = {
        {"sub",   userId},
        {"email", email},
        {"type",  userType},
        {"iat",   (long long)std::time(nullptr)},
        {"exp",   (long long)std::time(nullptr) + 86400}
    };

    std::string hdr_enc = base64UrlEncodeStr(header.dump());
    std::string pld_enc = base64UrlEncodeStr(payload.dump());
    std::string signing_input = hdr_enc + "." + pld_enc;

    auto sig = hmacSha256(signing_input, secret);
    std::string sig_enc = base64UrlEncode(sig.data(), sig.size());

    return signing_input + "." + sig_enc;
}

bool JwtHelper::validateToken(const std::string& token,
                              const std::string& secret,
                              nlohmann::json& out_payload) {
    // Split into 3 parts
    size_t p1 = token.find('.');
    if (p1 == std::string::npos) return false;
    size_t p2 = token.find('.', p1 + 1);
    if (p2 == std::string::npos) return false;

    std::string hdr_enc = token.substr(0, p1);
    std::string pld_enc = token.substr(p1 + 1, p2 - p1 - 1);
    std::string sig_enc = token.substr(p2 + 1);

    // Verify signature
    std::string signing_input = hdr_enc + "." + pld_enc;
    auto expected_sig = hmacSha256(signing_input, secret);
    std::string expected_sig_enc = base64UrlEncode(expected_sig.data(), expected_sig.size());

    if (expected_sig_enc != sig_enc) return false;

    // Decode payload
    auto pld_bytes = base64UrlDecode(pld_enc);
    std::string pld_str(pld_bytes.begin(), pld_bytes.end());

    try {
        out_payload = nlohmann::json::parse(pld_str);
    } catch (...) {
        return false;
    }

    // Check expiry
    if (out_payload.contains("exp")) {
        long long exp = out_payload["exp"].get<long long>();
        if (std::time(nullptr) > exp) return false;
    }

    return true;
}

// --------------------------------------------------------------------------
// RS256 for Google service account JWT
// --------------------------------------------------------------------------

std::string JwtHelper::createServiceAccountJwt(const std::string& clientEmail,
                                               const std::string& privateKeyPem,
                                               const std::string& scope,
                                               const std::string& audience) {
    long long now = (long long)std::time(nullptr);

    nlohmann::json header  = {{"alg", "RS256"}, {"typ", "JWT"}};
    nlohmann::json payload = {
        {"iss",   clientEmail},
        {"sub",   clientEmail},
        {"scope", scope},
        {"aud",   audience},
        {"iat",   now},
        {"exp",   now + 3600}
    };

    std::string hdr_enc = base64UrlEncodeStr(header.dump());
    std::string pld_enc = base64UrlEncodeStr(payload.dump());
    std::string signing_input = hdr_enc + "." + pld_enc;

    // Load private key
    BIO* bio = BIO_new_mem_buf(privateKeyPem.data(), (int)privateKeyPem.size());
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!pkey) {
        throw std::runtime_error("Failed to load RSA private key");
    }

    // Sign
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
    EVP_DigestSignUpdate(ctx,
        reinterpret_cast<const unsigned char*>(signing_input.data()),
        signing_input.size());

    size_t sig_len = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sig_len);
    std::vector<unsigned char> sig(sig_len);
    EVP_DigestSignFinal(ctx, sig.data(), &sig_len);
    sig.resize(sig_len);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    std::string sig_enc = base64UrlEncode(sig.data(), sig.size());
    return signing_input + "." + sig_enc;
}
