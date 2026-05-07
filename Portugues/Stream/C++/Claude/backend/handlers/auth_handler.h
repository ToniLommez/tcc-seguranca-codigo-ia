#pragma once
#include <httplib.h>
#include "firebase/firebase_service.h"

class AuthHandler {
public:
    explicit AuthHandler(FirebaseService& firebase);
    void registerRoutes(httplib::Server& svr);

private:
    void handleRegister(const httplib::Request& req, httplib::Response& res);
    void handleLogin(const httplib::Request& req, httplib::Response& res);

    FirebaseService& m_firebase;
};
