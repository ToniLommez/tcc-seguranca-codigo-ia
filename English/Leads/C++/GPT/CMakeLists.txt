cmake_minimum_required(VERSION 3.24)

project(gpt_cpp_lead_manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable Mbed TLS sample programs" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Disable Mbed TLS tests" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "Do not treat Mbed TLS warnings as errors" FORCE)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "Disable Mbed TLS package install logic" FORCE)

FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.16.3
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG mbedtls-3.6.2
)

FetchContent_Declare(
  tinyxml2
  GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
  GIT_TAG 10.0.0
)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json mbedtls tinyxml2)

add_executable(gpt_cpp_lead_manager
  src/app_config.cpp
  src/models.cpp
  src/utils.cpp
  src/http_client.cpp
  src/firestore_client.cpp
  src/auth_service.cpp
  src/lead_service.cpp
  src/main.cpp
)

target_include_directories(gpt_cpp_lead_manager PRIVATE include)

target_link_libraries(gpt_cpp_lead_manager PRIVATE
  httplib::httplib
  nlohmann_json::nlohmann_json
  mbedtls
  mbedx509
  mbedcrypto
  tinyxml2::tinyxml2
  crypt32
  ole32
  winhttp
  ws2_32
)

if(MINGW)
  target_link_libraries(gpt_cpp_lead_manager PRIVATE secur32)
endif()

add_custom_command(TARGET gpt_cpp_lead_manager POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/frontend
          $<TARGET_FILE_DIR:gpt_cpp_lead_manager>/frontend
)
