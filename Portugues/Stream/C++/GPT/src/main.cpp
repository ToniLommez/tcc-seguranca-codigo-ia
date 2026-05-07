#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "app_config.h"
#include "firebase_user_store.h"
#include "jwt_service.h"
#include "models.h"
#include "password_hasher.h"
#include "song_store.h"
#include "utils.h"

namespace {

struct ByteRange {
  std::size_t start = 0;
  std::size_t end = 0;
  bool partial = false;
};

std::optional<ByteRange> parse_range_header(const httplib::Request& req, std::size_t file_size) {
  if (!req.has_header("Range")) {
    return ByteRange{0, file_size == 0 ? 0 : file_size - 1, false};
  }

  const std::string header = req.get_header_value("Range");
  if (header.rfind("bytes=", 0) != 0) {
    return std::nullopt;
  }

  const std::string spec = header.substr(6);
  const auto dash = spec.find('-');
  if (dash == std::string::npos) {
    return std::nullopt;
  }

  const std::string start_text = spec.substr(0, dash);
  const std::string end_text = spec.substr(dash + 1);

  std::size_t start = 0;
  std::size_t end = file_size == 0 ? 0 : file_size - 1;

  try {
    if (start_text.empty()) {
      const auto suffix_length = static_cast<std::size_t>(std::stoull(end_text));
      start = suffix_length >= file_size ? 0 : file_size - suffix_length;
    } else {
      start = static_cast<std::size_t>(std::stoull(start_text));
    }

    if (!end_text.empty() && !start_text.empty()) {
      end = static_cast<std::size_t>(std::stoull(end_text));
    }
  } catch (...) {
    return std::nullopt;
  }

  if (start >= file_size || end >= file_size || start > end) {
    return std::nullopt;
  }

  return ByteRange{start, end, true};
}

std::optional<nlohmann::json> parse_json_body(const httplib::Request& req, httplib::Response& res) {
  try {
    return nlohmann::json::parse(req.body);
  } catch (const std::exception&) {
    utils::set_error(res, 400, "JSON invalido.");
    return std::nullopt;
  }
}

std::optional<AuthUser> authenticate_request(const httplib::Request& req,
                                             httplib::Response& res,
                                             const JwtService& jwt) {
  std::string token;

  if (req.has_header("Authorization")) {
    const std::string header = req.get_header_value("Authorization");
    constexpr const char* prefix = "Bearer ";
    if (header.rfind(prefix, 0) == 0) {
      token = header.substr(std::char_traits<char>::length(prefix));
    }
  }

  if (token.empty() && req.has_param("token")) {
    token = req.get_param_value("token");
  }

  if (token.empty()) {
    utils::set_error(res, 401, "Token JWT ausente.");
    return std::nullopt;
  }

  auto user = jwt.verify_token(token);
  if (!user.has_value()) {
    utils::set_error(res, 401, "Token JWT invalido ou expirado.");
    return std::nullopt;
  }

  return user;
}

std::optional<AuthUser> require_role(const httplib::Request& req,
                                     httplib::Response& res,
                                     const JwtService& jwt,
                                     UserType expected_type) {
  auto user = authenticate_request(req, res, jwt);
  if (!user.has_value()) {
    return std::nullopt;
  }

  if (user->type != expected_type) {
    utils::set_error(res, 403, "Perfil sem permissao para este recurso.");
    return std::nullopt;
  }

  return user;
}

} // namespace

int main(int argc, char* argv[]) {
  try {
    const std::filesystem::path config_path =
      argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("config/appsettings.json");

    AppConfig config = AppConfig::load_from_file(config_path);
    config.prepare_runtime();

    FirebaseUserStore user_store(config);
    SongStore song_store(config.songs_catalog_path);
    JwtService jwt_service(config.jwt_secret, config.jwt_expiration_hours);

    httplib::Server server;
    server.set_payload_max_length(config.max_upload_mb * 1024ULL * 1024ULL);
    server.set_read_timeout(std::chrono::seconds(30));
    server.set_write_timeout(std::chrono::seconds(30));
    server.set_idle_interval(std::chrono::milliseconds(100));

    server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
      std::cout << req.method << " " << req.path << " -> " << res.status << std::endl;
    });

    server.Get("/", [&config](const httplib::Request&, httplib::Response& res) {
      res.set_file_content((config.frontend_dir / "index.html").string(), "text/html; charset=utf-8");
    });

    server.Get("/styles.css", [&config](const httplib::Request&, httplib::Response& res) {
      res.set_file_content((config.frontend_dir / "styles.css").string(), "text/css; charset=utf-8");
    });

    server.Get("/app.js", [&config](const httplib::Request&, httplib::Response& res) {
      res.set_file_content((config.frontend_dir / "app.js").string(), "application/javascript; charset=utf-8");
    });

    server.Post("/api/auth/register", [&](const httplib::Request& req, httplib::Response& res) {
      const auto body = parse_json_body(req, res);
      if (!body.has_value()) {
        return;
      }

      const std::string name = utils::trim(body->value("name", ""));
      const std::string email = utils::to_lower(utils::trim(body->value("email", "")));
      const std::string password = body->value("password", "");
      const auto type = user_type_from_string(body->value("type", ""));

      if (name.empty() || email.empty() || password.empty() || !type.has_value()) {
        utils::set_error(res, 400, "Informe nome, e-mail, senha e tipo valido.");
        return;
      }

      if (password.size() < 6) {
        utils::set_error(res, 400, "A senha deve ter pelo menos 6 caracteres.");
        return;
      }

      if (user_store.find_by_email(email).has_value()) {
        utils::set_error(res, 409, "Ja existe um usuario cadastrado com este e-mail.");
        return;
      }

      UserRecord user{
        name,
        email,
        PasswordHasher::hash_password(password),
        *type,
        utils::utc_now_iso8601()
      };

      std::string error_message;
      if (!user_store.create_user(user, error_message)) {
        utils::set_error(res, 500, error_message);
        return;
      }

      utils::set_json(res,
                      {
                        {"message", "Usuario cadastrado com sucesso."},
                        {"user", to_public_json(user)}
                      },
                      201);
    });

    server.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
      const auto body = parse_json_body(req, res);
      if (!body.has_value()) {
        return;
      }

      const std::string email = utils::to_lower(utils::trim(body->value("email", "")));
      const std::string password = body->value("password", "");

      if (email.empty() || password.empty()) {
        utils::set_error(res, 400, "Informe e-mail e senha.");
        return;
      }

      const auto user = user_store.find_by_email(email);
      if (!user.has_value() || !PasswordHasher::verify_password(password, user->password_hash)) {
        utils::set_error(res, 401, "Credenciais invalidas.");
        return;
      }

      const AuthUser auth_user{user->name, user->email, user->type};
      const std::string token = jwt_service.issue_token(auth_user);

      utils::set_json(res,
                      {
                        {"token", token},
                        {"user", to_public_json(auth_user)}
                      });
    });

    server.Get("/api/me", [&](const httplib::Request& req, httplib::Response& res) {
      const auto user = authenticate_request(req, res, jwt_service);
      if (!user.has_value()) {
        return;
      }

      utils::set_json(res, {{"user", to_public_json(*user)}});
    });

    server.Post("/api/songs", [&](const httplib::Request& req, httplib::Response& res) {
      const auto artist = require_role(req, res, jwt_service, UserType::Artist);
      if (!artist.has_value()) {
        return;
      }

      if (!req.is_multipart_form_data()) {
        utils::set_error(res, 400, "Envie os dados em multipart/form-data.");
        return;
      }

      const std::string title = utils::trim(req.form.get_field("title"));
      const std::string genre = utils::trim(req.form.get_field("genre"));
      const std::string description = utils::trim(req.form.get_field("description"));

      if (title.empty() || genre.empty()) {
        utils::set_error(res, 400, "Informe titulo e genero.");
        return;
      }

      if (!req.form.has_file("audio")) {
        utils::set_error(res, 400, "Selecione um arquivo MP3.");
        return;
      }

      const auto& file = req.form.get_file("audio");
      const auto filename_lower = utils::to_lower(file.filename);
      if (filename_lower.size() < 4 || filename_lower.substr(filename_lower.size() - 4) != ".mp3") {
        utils::set_error(res, 400, "Apenas arquivos MP3 sao permitidos.");
        return;
      }

      const std::string song_id = utils::random_hex(12);
      const std::string saved_file_name = song_id + ".mp3";
      const auto absolute_file_path = config.uploads_dir / saved_file_name;

      try {
        utils::write_binary_file(absolute_file_path, file.content);
      } catch (const std::exception& ex) {
        utils::set_error(res, 500, ex.what());
        return;
      }

      SongRecord song{
        song_id,
        title,
        genre,
        description,
        artist->email,
        artist->name,
        saved_file_name,
        (std::filesystem::path("uploads") / saved_file_name).generic_string(),
        utils::utc_now_iso8601()
      };

      song_store.add_song(song);

      utils::set_json(res,
                      {
                        {"message", "Musica cadastrada com sucesso."},
                        {"song", to_json(song)}
                      },
                      201);
    });

    server.Get("/api/songs/mine", [&](const httplib::Request& req, httplib::Response& res) {
      const auto artist = require_role(req, res, jwt_service, UserType::Artist);
      if (!artist.has_value()) {
        return;
      }

      const auto songs = song_store.list_by_artist(artist->email);
      nlohmann::json items = nlohmann::json::array();
      for (const auto& song : songs) {
        items.push_back(to_json(song));
      }

      utils::set_json(res, {{"songs", items}});
    });

    server.Get("/api/songs", [&](const httplib::Request& req, httplib::Response& res) {
      const auto user = require_role(req, res, jwt_service, UserType::User);
      if (!user.has_value()) {
        return;
      }

      const std::string title = req.has_param("nome") ? req.get_param_value("nome") : "";
      const std::string artist = req.has_param("artista") ? req.get_param_value("artista") : "";
      const std::string genre = req.has_param("genero") ? req.get_param_value("genero") : "";

      const auto songs = song_store.search(title, artist, genre);
      nlohmann::json items = nlohmann::json::array();
      const std::string query_token = req.has_param("token") ? req.get_param_value("token") : "";

      for (const auto& song : songs) {
        auto song_json = to_json(song);
        song_json["streamUrl"] = "/api/songs/" + song.id + "/stream?token=" + query_token;
        items.push_back(song_json);
      }

      utils::set_json(res, {{"songs", items}});
    });

    server.Get(R"(/api/songs/([0-9a-f]+)/stream)", [&](const httplib::Request& req, httplib::Response& res) {
      const auto user = require_role(req, res, jwt_service, UserType::User);
      if (!user.has_value()) {
        return;
      }

      const std::string song_id = req.matches[1];
      const auto song = song_store.find_by_id(song_id);
      if (!song.has_value()) {
        utils::set_error(res, 404, "Musica nao encontrada.");
        return;
      }

      const auto absolute_file_path = config.uploads_dir / song->file_name;
      if (!std::filesystem::exists(absolute_file_path)) {
        utils::set_error(res, 404, "Arquivo de audio nao encontrado no servidor.");
        return;
      }

      const auto file_size = static_cast<std::size_t>(std::filesystem::file_size(absolute_file_path));
      const auto range = parse_range_header(req, file_size);
      if (!range.has_value()) {
        res.status = 416;
        res.set_header("Content-Range", "bytes */" + std::to_string(file_size));
        return;
      }

      auto file_stream = std::make_shared<std::ifstream>(absolute_file_path, std::ios::binary);
      if (!file_stream->is_open()) {
        utils::set_error(res, 500, "Nao foi possivel abrir o arquivo para streaming.");
        return;
      }

      const std::size_t content_length = range->end - range->start + 1;

      res.set_header("Accept-Ranges", "bytes");
      res.set_header("Content-Disposition", "inline; filename=\"" + song->file_name + "\"");
      if (range->partial) {
        res.status = 206;
        res.set_header("Content-Range",
                       "bytes " + std::to_string(range->start) + "-" + std::to_string(range->end) +
                         "/" + std::to_string(file_size));
      }

      res.set_content_provider(
        content_length,
        "audio/mpeg",
        [file_stream, start = range->start, content_length](std::size_t offset,
                                                            std::size_t length,
                                                            httplib::DataSink& sink) {
          if (offset >= content_length) {
            return true;
          }

          const auto remaining = content_length - offset;
          const auto to_read = std::min<std::size_t>(std::min<std::size_t>(length, 64 * 1024), remaining);
          std::vector<char> buffer(to_read);

          file_stream->clear();
          file_stream->seekg(static_cast<std::streamoff>(start + offset), std::ios::beg);
          file_stream->read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

          const auto read_bytes = static_cast<std::size_t>(file_stream->gcount());
          if (read_bytes == 0) {
            return false;
          }

          sink.write(buffer.data(), read_bytes);
          return true;
        },
        [file_stream](bool) {
          file_stream->close();
        });
    });

    server.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
      if (req.path.rfind("/api/", 0) == 0 && res.status == 404) {
        utils::set_error(res, 404, "Endpoint nao encontrado.");
      }
    });

    std::cout << "Servidor iniciado em http://" << config.host << ":" << config.port << std::endl;
    server.listen(config.host.c_str(), config.port);
  } catch (const std::exception& ex) {
    std::cerr << "Erro fatal: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
