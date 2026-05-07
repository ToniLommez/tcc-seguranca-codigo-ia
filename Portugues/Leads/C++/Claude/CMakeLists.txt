cmake_minimum_required(VERSION 3.20)
project(LeadManager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages (installed via vcpkg)
find_package(Crow CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

set(SOURCES
    src/main.cpp
    src/firebase/firebase_client.cpp
    src/auth/auth_routes.cpp
    src/leads/leads_routes.cpp
    src/utils/jwt_utils.cpp
    src/utils/csv_utils.cpp
)

add_executable(lead-manager ${SOURCES})

target_include_directories(lead-manager PRIVATE src)

# Windows / MinGW compile definitions
if(WIN32)
    target_compile_definitions(lead-manager PRIVATE
        _WIN32_WINNT=0x0601   # Windows 7+
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

target_link_libraries(lead-manager PRIVATE
    Crow::Crow
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(lead-manager PRIVATE ws2_32 wsock32 mswsock crypt32 bcrypt)
endif()

# MinGW: ensure we link the C++ runtime statically so the exe is portable
if(MINGW)
    target_link_options(lead-manager PRIVATE -static-libgcc -static-libstdc++)
endif()

# Copy frontend to build directory
add_custom_command(TARGET lead-manager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/frontend"
    "$<TARGET_FILE_DIR:lead-manager>/frontend"
    COMMENT "Copying frontend files..."
)

# Copy credentials file to build directory
add_custom_command(TARGET lead-manager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Users/tonil/Desktop/tcc/lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
    "$<TARGET_FILE_DIR:lead-manager>/firebase-credentials.json"
    COMMENT "Copying Firebase credentials..."
)
