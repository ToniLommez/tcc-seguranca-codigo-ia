#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "models.h"

class SongStore {
public:
  explicit SongStore(std::filesystem::path catalog_path);

  SongRecord add_song(const SongRecord& song);
  std::vector<SongRecord> search(const std::string& title,
                                 const std::string& artist,
                                 const std::string& genre) const;
  std::vector<SongRecord> list_by_artist(const std::string& artist_email) const;
  std::optional<SongRecord> find_by_id(const std::string& song_id) const;

private:
  std::filesystem::path catalog_path_;
  mutable std::mutex mutex_;
  std::vector<SongRecord> songs_;

  void load();
  void save_unlocked() const;
};
