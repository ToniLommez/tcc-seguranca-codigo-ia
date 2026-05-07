cmake_minimum_required(VERSION 3.20)

project(lead_manager_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(lead_manager
    src/main.cpp
    src/app_config.cpp
    src/security.cpp
    src/http_client.cpp
    src/firebase_client.cpp
    src/csv_utils.cpp
    src/utils.cpp
    src/server.cpp
)

target_include_directories(lead_manager PRIVATE
    include
    external
)

target_compile_definitions(lead_manager PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(lead_manager PRIVATE
    winhttp
    bcrypt
    crypt32
    ncrypt
    rpcrt4
    ws2_32
)

file(COPY public DESTINATION ${CMAKE_BINARY_DIR})
configure_file(config.json ${CMAKE_BINARY_DIR}/config.json COPYONLY)
