cmake_minimum_required(VERSION 3.20)
project(SoundStream VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

add_executable(soundstream src/main.cpp)

target_include_directories(soundstream PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(soundstream PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
)

if(WIN32)
    target_link_libraries(soundstream PRIVATE ws2_32)
endif()

file(COPY ${PROJECT_SOURCE_DIR}/frontend DESTINATION ${PROJECT_BINARY_DIR})
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/uploads)
