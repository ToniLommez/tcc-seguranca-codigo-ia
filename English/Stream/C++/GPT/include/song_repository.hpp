#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "models.hpp"

class SongRepository {
  public:
    explicit SongRepository(std::filesystem::path storage_path);

    bool initialize(std::string& error);
    bool add_song(const Song& song, std::string& error);
    std::vector<Song> list_all() const;
    std::vector<Song> list_by_artist(const std::string& artist_id) const;
    std::vector<Song> search(
        const std::string& title_filter,
        const std::string& artist_filter,
        const std::string& genre_filter
    ) const;
    std::optional<Song> find_by_id(const std::string& song_id) const;

  private:
    bool save_locked(std::string& error);

    std::filesystem::path storage_path_;
    mutable std::mutex mutex_;
    std::vector<Song> songs_;
};

