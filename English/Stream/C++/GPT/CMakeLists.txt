cmake_minimum_required(VERSION 3.20)

project(StreamMusicService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

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

FetchContent_MakeAvailable(cpp_httplib nlohmann_json)

add_executable(stream_music_service
  src/app_config.cpp
  src/firebase_client.cpp
  src/jwt_service.cpp
  src/main.cpp
  src/password_service.cpp
  src/song_repository.cpp
  src/utils.cpp
)

target_include_directories(stream_music_service PRIVATE include)

target_link_libraries(stream_music_service
  PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    bcrypt
    crypt32
    ncrypt
    winhttp
    ws2_32
)

target_compile_definitions(stream_music_service PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

