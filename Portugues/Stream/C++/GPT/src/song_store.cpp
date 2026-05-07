#include "song_store.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "utils.h"

SongStore::SongStore(std::filesystem::path catalog_path) : catalog_path_(std::move(catalog_path)) {
  load();
}

void SongStore::load() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!std::filesystem::exists(catalog_path_)) {
    std::ofstream(catalog_path_) << "[]";
  }

  std::ifstream stream(catalog_path_);
  if (!stream) {
    throw std::runtime_error("Nao foi possivel abrir o catalogo local de musicas.");
  }

  const auto json = nlohmann::json::parse(stream);
  songs_.clear();

  for (const auto& item : json) {
    songs_.push_back(SongRecord{
      item.value("id", ""),
      item.value("title", ""),
      item.value("genre", ""),
      item.value("description", ""),
      item.value("artistEmail", ""),
      item.value("artistName", ""),
      item.value("fileName", ""),
      item.value("filePath", ""),
      item.value("createdAt", "")
    });
  }
}

void SongStore::save_unlocked() const {
  nlohmann::json payload = nlohmann::json::array();
  for (const auto& song : songs_) {
    payload.push_back(to_json(song));
  }

  const auto temp_path = catalog_path_.string() + ".tmp";
  utils::write_text_file(temp_path, payload.dump(2));

  if (std::filesystem::exists(catalog_path_)) {
    std::filesystem::remove(catalog_path_);
  }
  std::filesystem::rename(temp_path, catalog_path_);
}

SongRecord SongStore::add_song(const SongRecord& song) {
  std::lock_guard<std::mutex> lock(mutex_);
  songs_.push_back(song);
  save_unlocked();
  return song;
}

std::vector<SongRecord> SongStore::search(const std::string& title,
                                          const std::string& artist,
                                          const std::string& genre) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<SongRecord> result;

  for (const auto& song : songs_) {
    const bool title_ok = title.empty() || utils::icontains(song.title, title);
    const bool artist_ok = artist.empty() || utils::icontains(song.artist_name, artist);
    const bool genre_ok = genre.empty() || utils::icontains(song.genre, genre);

    if (title_ok && artist_ok && genre_ok) {
      result.push_back(song);
    }
  }

  std::sort(result.begin(), result.end(), [](const SongRecord& a, const SongRecord& b) {
    return a.created_at > b.created_at;
  });

  return result;
}

std::vector<SongRecord> SongStore::list_by_artist(const std::string& artist_email) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<SongRecord> result;

  for (const auto& song : songs_) {
    if (utils::to_lower(song.artist_email) == utils::to_lower(artist_email)) {
      result.push_back(song);
    }
  }

  std::sort(result.begin(), result.end(), [](const SongRecord& a, const SongRecord& b) {
    return a.created_at > b.created_at;
  });

  return result;
}

std::optional<SongRecord> SongStore::find_by_id(const std::string& song_id) const {
  std::lock_guard<std::mutex> lock(mutex_);

  for (const auto& song : songs_) {
    if (song.id == song_id) {
      return song;
    }
  }

  return std::nullopt;
}
