#pragma once

#include <httplib.h>
#include "firebase_client.h"

class LeadController {
public:
    LeadController(FirebaseClient& firebase);

    void create(const httplib::Request& req, httplib::Response& res);
    void get_all(const httplib::Request& req, httplib::Response& res);
    void get_one(const httplib::Request& req, httplib::Response& res);
    void update(const httplib::Request& req, httplib::Response& res);
    void remove(const httplib::Request& req, httplib::Response& res);
    void export_csv(const httplib::Request& req, httplib::Response& res);
    void import_csv(const httplib::Request& req, httplib::Response& res);
    void dashboard(const httplib::Request& req, httplib::Response& res);

private:
    FirebaseClient& firebase_;
    static constexpr const char* COLLECTION = "Claude_Cpp_Leads";
    std::string get_timestamp() const;
};
