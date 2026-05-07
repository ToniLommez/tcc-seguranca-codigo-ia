#pragma once

#include <httplib.h>
#include "firebase_client.h"

class AuthController {
public:
    AuthController(FirebaseClient& firebase);

    void signup(const httplib::Request& req, httplib::Response& res);
    void login(const httplib::Request& req, httplib::Response& res);

    static std::string extract_user_id(const httplib::Request& req);

private:
    FirebaseClient& firebase_;
    static constexpr const char* COLLECTION = "Claude_Cpp_Users";
};
