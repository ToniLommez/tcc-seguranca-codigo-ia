cmake_minimum_required(VERSION 3.20)
project(MusicStreamingServer VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required for CreateFile2, GetAddrInfoExCancel and other Win8+ APIs used by httplib
add_compile_definitions(_WIN32_WINNT=0x0A00)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

set(SOURCES
    backend/main.cpp
    backend/auth/jwt_helper.cpp
    backend/firebase/firebase_service.cpp
    backend/db/local_db.cpp
    backend/handlers/auth_handler.cpp
    backend/handlers/music_handler.cpp
    backend/handlers/stream_handler.cpp
)

add_executable(music_server ${SOURCES})

target_include_directories(music_server PRIVATE backend)

target_link_libraries(music_server PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    unofficial::sqlite3::sqlite3
    nlohmann_json::nlohmann_json
    httplib::httplib
)

add_custom_command(TARGET music_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/frontend
        $<TARGET_FILE_DIR:music_server>/frontend
    COMMENT "Copying frontend files..."
)

add_custom_command(TARGET music_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:music_server>/uploads
    COMMENT "Creating uploads directory..."
)

# Copy vcpkg DLLs so the exe runs without PATH changes
add_custom_command(TARGET music_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-mingw-dynamic/bin
        $<TARGET_FILE_DIR:music_server>
    COMMENT "Copying runtime DLLs (release)..."
)
add_custom_command(TARGET music_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-mingw-dynamic/debug/bin
        $<TARGET_FILE_DIR:music_server>
    COMMENT "Copying runtime DLLs (debug)..."
)
