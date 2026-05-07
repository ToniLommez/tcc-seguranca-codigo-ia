cmake_minimum_required(VERSION 3.21)
project(stream_music LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.24.0
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json)

if(DEFINED OPENSSL_INCLUDE_DIR AND DEFINED OPENSSL_SSL_LIBRARY AND DEFINED OPENSSL_CRYPTO_LIBRARY)
  add_library(OpenSSL::SSL UNKNOWN IMPORTED)
  set_target_properties(OpenSSL::SSL PROPERTIES
    IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
  )

  add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
  set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
  )
else()
  find_package(OpenSSL REQUIRED)
endif()

add_executable(stream_music
  src/app_config.cpp
  src/firebase_user_store.cpp
  src/jwt_service.cpp
  src/main.cpp
  src/password_hasher.cpp
  src/song_store.cpp
  src/utils.cpp
)

target_include_directories(stream_music PRIVATE src)

target_link_libraries(stream_music
  PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
  target_link_libraries(stream_music PRIVATE ws2_32 crypt32)
  target_compile_definitions(stream_music PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

target_compile_definitions(stream_music PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
