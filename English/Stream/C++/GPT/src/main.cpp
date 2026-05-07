#include <filesystem>
#include <iostream>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "app_config.hpp"
#include "firebase_client.hpp"
#include "jwt_service.hpp"
#include "password_service.hpp"
#include "song_repository.hpp"
#include "utils.hpp"

namespace {

using json = nlohmann::json;

void json_response(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(2), "application/json");
}

std::optional<json> parse_json_request(const httplib::Request& req, httplib::Response& res) {
    const auto body = json::parse(req.body, nullptr, false);
    if (body.is_discarded()) {
        json_response(res, 400, {{"error", "Invalid JSON request body."}});
        return std::nullopt;
    }

    return body;
}

std::optional<JwtClaims> authorize_request(
    const httplib::Request& req,
    httplib::Response& res,
    const JwtService& jwt_service,
    std::optional<UserRole> required_role = std::nullopt
) {
    std::string token;
    const auto auth_header = req.get_header_value("Authorization");

    if (!auth_header.empty() && auth_header.rfind("Bearer ", 0) == 0) {
        token = auth_header.substr(7);
    } else if (req.has_param("token")) {
        token = req.get_param_value("token");
    }

    if (token.empty()) {
        json_response(res, 401, {{"error", "Authentication token is required."}});
        return std::nullopt;
    }

    auto claims = jwt_service.verify_token(token);
    if (!claims.has_value()) {
        json_response(res, 401, {{"error", "Authentication token is invalid or expired."}});
        return std::nullopt;
    }

    if (required_role.has_value() && claims->role != *required_role) {
        json_response(res, 403, {{"error", "You do not have permission to access this resource."}});
        return std::nullopt;
    }

    return claims;
}

bool serve_file(const std::filesystem::path& path, const std::string& content_type, httplib::Response& res) {
    std::string error;
    const auto contents = util::read_text_file(path, error);
    if (!contents.has_value()) {
        res.status = 404;
        res.set_content("Not found", "text/plain");
        return false;
    }

    res.set_content(*contents, content_type.c_str());
    return true;
}

std::string extract_request_value(const httplib::Request& req, const std::string& key) {
    if (req.has_param(key)) {
        return util::trim_copy(req.get_param_value(key));
    }

    return {};
}

}  // namespace

int main() {
    try {
        const auto root = std::filesystem::current_path();
        const auto config = AppConfig::load(root / "appsettings.json");
        const auto frontend_dir = std::filesystem::absolute(root / config.frontend_dir);
        const auto uploads_dir = std::filesystem::absolute(root / config.uploads_dir);
        const auto songs_data_path = std::filesystem::absolute(root / config.songs_data_path);

        std::string error;
        if (!util::ensure_directory(uploads_dir, error)) {
            std::cerr << "Could not create uploads directory: " << error << '\n';
            return 1;
        }

        SongRepository song_repository(songs_data_path);
        if (!song_repository.initialize(error)) {
            std::cerr << "Could not initialize song repository: " << error << '\n';
            return 1;
        }

        FirebaseClient firebase_client(config);
        if (!firebase_client.initialize(error)) {
            std::cerr << "Could not initialize Firebase client: " << error << '\n';
            return 1;
        }

        JwtService jwt_service(config.jwt_secret);
        httplib::Server server;
        server.set_file_extension_and_mimetype_mapping("css", "text/css");
        server.set_file_extension_and_mimetype_mapping("js", "application/javascript");
        server.set_file_extension_and_mimetype_mapping("html", "text/html");

        server.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
            res.status = 204;
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        });

        server.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            return httplib::Server::HandlerResponse::Unhandled;
        });

        server.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
            json_response(res, 200, {{"status", "ok"}});
        });

        server.Post("/api/auth/register", [&](const httplib::Request& req, httplib::Response& res) {
            const auto body = parse_json_request(req, res);
            if (!body.has_value()) {
                return;
            }

            const auto name = util::trim_copy(body->value("name", ""));
            const auto email = util::lowercase_copy(util::trim_copy(body->value("email", "")));
            const auto password = body->value("password", "");
            const auto role = role_from_string(body->value("type", ""));

            if (name.empty() || email.empty() || password.size() < 6 || !role.has_value()) {
                json_response(
                    res,
                    400,
                    {{"error", "Registration requires name, email, password (min 6), and type ARTIST or USER."}}
                );
                return;
            }

            UserRecord user;
            user.id = util::generate_id();
            user.name = name;
            user.email = email;
            user.role = *role;
            user.created_at = util::current_utc_timestamp();

            std::string password_error;
            if (!PasswordService::hash_password(password, user.password_salt, user.password_hash, password_error)) {
                json_response(res, 500, {{"error", "Could not secure the password."}});
                return;
            }

            std::string firebase_error;
            if (!firebase_client.register_user(user, firebase_error)) {
                const auto status = util::icontains(firebase_error, "already exists") ? 409 : 500;
                json_response(res, status, {{"error", firebase_error}});
                return;
            }

            json_response(
                res,
                201,
                {{"message", "User registered successfully."},
                 {"user",
                  {{"id", user.id}, {"name", user.name}, {"email", user.email}, {"type", role_to_string(user.role)}}}}
            );
        });

        server.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
            const auto body = parse_json_request(req, res);
            if (!body.has_value()) {
                return;
            }

            const auto email = util::lowercase_copy(util::trim_copy(body->value("email", "")));
            const auto password = body->value("password", "");

            if (email.empty() || password.empty()) {
                json_response(res, 400, {{"error", "Email and password are required."}});
                return;
            }

            std::string firebase_error;
            const auto user = firebase_client.find_user_by_email(email, firebase_error);
            if (!user.has_value()) {
                const auto message = firebase_error.empty() ? "Invalid email or password." : firebase_error;
                const auto status = firebase_error.empty() ? 401 : 500;
                json_response(res, status, {{"error", message}});
                return;
            }

            std::string password_error;
            const bool password_ok = PasswordService::verify_password(
                password,
                user->password_salt,
                user->password_hash,
                password_error
            );

            if (!password_ok) {
                if (!password_error.empty()) {
                    json_response(res, 500, {{"error", password_error}});
                    return;
                }

                json_response(res, 401, {{"error", "Invalid email or password."}});
                return;
            }

            const auto token = jwt_service.create_token(*user);
            json_response(
                res,
                200,
                {{"token", token},
                 {"user",
                  {{"id", user->id},
                   {"name", user->name},
                   {"email", user->email},
                   {"type", role_to_string(user->role)}}}}
            );
        });

        server.Get("/api/me", [&](const httplib::Request& req, httplib::Response& res) {
            const auto claims = authorize_request(req, res, jwt_service);
            if (!claims.has_value()) {
                return;
            }

            json_response(
                res,
                200,
                {{"id", claims->subject},
                 {"name", claims->name},
                 {"email", claims->email},
                 {"type", role_to_string(claims->role)}}
            );
        });

        server.Post("/api/artist/songs", [&](const httplib::Request& req, httplib::Response& res) {
            const auto claims = authorize_request(req, res, jwt_service, UserRole::Artist);
            if (!claims.has_value()) {
                return;
            }

            const auto title = extract_request_value(req, "title");
            const auto genre = extract_request_value(req, "genre");
            const auto description = extract_request_value(req, "description");

            if (title.empty() || genre.empty()) {
                json_response(res, 400, {{"error", "Title and genre are required."}});
                return;
            }

            if (!req.has_file("musicFile")) {
                json_response(res, 400, {{"error", "The MP3 file is required."}});
                return;
            }

            const auto file = req.get_file_value("musicFile");
            const auto filename = util::sanitize_filename(file.filename);
            const auto lowercase_name = util::lowercase_copy(filename);

            if (lowercase_name.size() < 4 || lowercase_name.substr(lowercase_name.size() - 4) != ".mp3") {
                json_response(res, 400, {{"error", "Only MP3 files are accepted."}});
                return;
            }

            Song song;
            song.id = util::generate_id();
            song.title = title;
            song.genre = genre;
            song.description = description;
            song.artist_id = claims->subject;
            song.artist_name = claims->name;
            song.original_file_name = filename;
            song.uploaded_at = util::current_utc_timestamp();

            const auto stored_name = song.id + ".mp3";
            const auto absolute_file_path = uploads_dir / stored_name;
            song.file_path = absolute_file_path.string();

            std::string save_error;
            if (!util::write_binary_file(absolute_file_path, file.content, save_error)) {
                json_response(res, 500, {{"error", save_error}});
                return;
            }

            std::string repository_error;
            if (!song_repository.add_song(song, repository_error)) {
                std::filesystem::remove(absolute_file_path);
                json_response(res, 500, {{"error", repository_error}});
                return;
            }

            json_response(
                res,
                201,
                {{"message", "Song uploaded successfully."}, {"song", song.to_json()}}
            );
        });

        server.Get("/api/artist/songs", [&](const httplib::Request& req, httplib::Response& res) {
            const auto claims = authorize_request(req, res, jwt_service, UserRole::Artist);
            if (!claims.has_value()) {
                return;
            }

            const auto songs = song_repository.list_by_artist(claims->subject);
            json song_list = json::array();
            for (const auto& song : songs) {
                song_list.push_back(song.to_json());
            }

            json_response(res, 200, {{"songs", std::move(song_list)}});
        });

        server.Get("/api/songs", [&](const httplib::Request& req, httplib::Response& res) {
            const auto claims = authorize_request(req, res, jwt_service, UserRole::User);
            if (!claims.has_value()) {
                return;
            }

            const auto title = extract_request_value(req, "title");
            const auto artist = extract_request_value(req, "artist");
            const auto genre = extract_request_value(req, "genre");

            const auto songs = song_repository.search(title, artist, genre);
            json song_list = json::array();
            for (const auto& song : songs) {
                song_list.push_back(song.to_json());
            }

            json_response(res, 200, {{"songs", std::move(song_list)}});
        });

        server.Get(R"(/api/songs/([\w\-]+)/stream)", [&](const httplib::Request& req, httplib::Response& res) {
            const auto claims = authorize_request(req, res, jwt_service, UserRole::User);
            if (!claims.has_value()) {
                return;
            }

            const auto song_id = std::string(req.matches[1]);
            const auto song = song_repository.find_by_id(song_id);
            if (!song.has_value()) {
                json_response(res, 404, {{"error", "Song not found."}});
                return;
            }

            const std::filesystem::path file_path(song->file_path);
            const auto total_size = util::file_size_or_zero(file_path);
            if (total_size == 0) {
                json_response(res, 404, {{"error", "Audio file not found on disk."}});
                return;
            }

            std::uint64_t start = 0;
            std::uint64_t end = total_size - 1;
            const auto range_header = req.get_header_value("Range");

            if (!range_header.empty() && range_header.rfind("bytes=", 0) == 0) {
                const auto range_value = range_header.substr(6);
                const auto dash_index = range_value.find('-');

                if (dash_index != std::string::npos) {
                    const auto start_text = range_value.substr(0, dash_index);
                    const auto end_text = range_value.substr(dash_index + 1);

                    if (!start_text.empty()) {
                        start = static_cast<std::uint64_t>(std::stoull(start_text));
                    }

                    if (!end_text.empty()) {
                        end = static_cast<std::uint64_t>(std::stoull(end_text));
                    }
                }

                if (end >= total_size) {
                    end = total_size - 1;
                }

                if (start > end || start >= total_size) {
                    res.status = 416;
                    res.set_header("Content-Range", "bytes */" + std::to_string(total_size));
                    return;
                }
            }

            const auto length = end - start + 1;
            std::string read_error;
            const auto audio = util::read_binary_range(file_path, start, length, read_error);
            if (!audio.has_value()) {
                json_response(res, 500, {{"error", read_error}});
                return;
            }

            res.set_header("Accept-Ranges", "bytes");
            res.set_header("Content-Type", "audio/mpeg");

            if (!range_header.empty()) {
                res.status = 206;
                res.set_header(
                    "Content-Range",
                    "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(total_size)
                );
            } else {
                res.status = 200;
            }

            res.set_content(*audio, "audio/mpeg");
        });

        server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
            serve_file(frontend_dir / "index.html", "text/html", res);
        });

        server.Get("/styles.css", [&](const httplib::Request&, httplib::Response& res) {
            serve_file(frontend_dir / "styles.css", "text/css", res);
        });

        server.Get("/app.js", [&](const httplib::Request&, httplib::Response& res) {
            serve_file(frontend_dir / "app.js", "application/javascript", res);
        });

        std::cout << "Stream Music Service running on http://localhost:" << config.port << '\n';
        if (!server.listen("0.0.0.0", config.port)) {
            std::cerr << "Could not bind to port " << config.port
                      << ". Set a different value in appsettings.json or the STREAM_APP_PORT environment variable.\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "Unhandled error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
