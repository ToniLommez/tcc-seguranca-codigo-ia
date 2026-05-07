#pragma once
#include <crow.h>
#include <memory>
#include "../firebase/firebase_client.h"

void setupAuthRoutes(crow::SimpleApp& app,
                     std::shared_ptr<FirebaseClient> firebase);
