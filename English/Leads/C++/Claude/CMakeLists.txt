cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(LeadManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_TLS_VERIFY OFF)
set(FETCHCONTENT_TLS_VERIFY OFF)

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
    GIT_CONFIG     http.sslVerify=false
)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_CONFIG     http.sslVerify=false
    OVERRIDE_FIND_PACKAGE
)

set(JWT_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JWT_CPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
    GIT_CONFIG     http.sslVerify=false
)

find_package(OpenSSL REQUIRED)

set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nlohmann_json httplib jwt-cpp)

add_executable(lead_manager
    src/main.cpp
    src/firebase_client.cpp
    src/auth_controller.cpp
    src/lead_controller.cpp
    src/jwt_utils.cpp
    src/csv_utils.cpp
)

target_include_directories(lead_manager PRIVATE src)

target_link_libraries(lead_manager PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(lead_manager PRIVATE ws2_32 crypt32)
endif()

target_compile_definitions(lead_manager PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)
