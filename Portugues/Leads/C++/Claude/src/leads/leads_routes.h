#pragma once
#include <crow.h>
#include <memory>
#include "../firebase/firebase_client.h"

void setupLeadsRoutes(crow::SimpleApp& app,
                      std::shared_ptr<FirebaseClient> firebase);
