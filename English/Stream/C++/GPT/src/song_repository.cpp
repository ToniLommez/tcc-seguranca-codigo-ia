#include "song_repository.hpp"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

#include "utils.hpp"

SongRepository::SongRepository(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {}

bool SongRepository::initialize(std::string& error) {
    const auto parent = storage_path_.parent_path();
    if (!parent.empty() && !util::ensure_directory(parent, error)) {
        return false;
    }

    if (!util::file_exists(storage_path_)) {
        if (!util::write_text_file(storage_path_, "[]", error)) {
            return false;
        }
    }

    const auto contents = util::read_text_file(storage_path_, error);
    if (!contents.has_value()) {
        return false;
    }

    const auto json = nlohmann::json::parse(*contents, nullptr, false);
    if (!json.is_array()) {
        error = "Songs data file is invalid.";
        return false;
    }

    std::scoped_lock lock(mutex_);
    songs_.clear();

    for (const auto& item : json) {
        Song song;
        song.id = item.value("id", "");
        song.title = item.value("title", "");
        song.genre = item.value("genre", "");
        song.description = item.value("description", "");
        song.artist_id = item.value("artistId", "");
        song.artist_name = item.value("artistName", "");
        song.file_path = item.value("filePath", "");
        song.original_file_name = item.value("originalFileName", "");
        song.uploaded_at = item.value("uploadedAt", "");

        if (!song.id.empty()) {
            songs_.push_back(std::move(song));
        }
    }

    return true;
}

bool SongRepository::add_song(const Song& song, std::string& error) {
    std::scoped_lock lock(mutex_);
    songs_.push_back(song);
    return save_locked(error);
}

std::vector<Song> SongRepository::list_all() const {
    std::scoped_lock lock(mutex_);
    return songs_;
}

std::vector<Song> SongRepository::list_by_artist(const std::string& artist_id) const {
    std::scoped_lock lock(mutex_);
    std::vector<Song> result;

    for (const auto& song : songs_) {
        if (song.artist_id == artist_id) {
            result.push_back(song);
        }
    }

    return result;
}

std::vector<Song> SongRepository::search(
    const std::string& title_filter,
    const std::string& artist_filter,
    const std::string& genre_filter
) const {
    std::scoped_lock lock(mutex_);
    std::vector<Song> result;

    for (const auto& song : songs_) {
        const bool title_ok = title_filter.empty() || util::icontains(song.title, title_filter);
        const bool artist_ok = artist_filter.empty() || util::icontains(song.artist_name, artist_filter);
        const bool genre_ok = genre_filter.empty() || util::icontains(song.genre, genre_filter);

        if (title_ok && artist_ok && genre_ok) {
            result.push_back(song);
        }
    }

    return result;
}

std::optional<Song> SongRepository::find_by_id(const std::string& song_id) const {
    std::scoped_lock lock(mutex_);

    const auto iterator = std::find_if(songs_.begin(), songs_.end(), [&](const Song& song) {
        return song.id == song_id;
    });

    if (iterator == songs_.end()) {
        return std::nullopt;
    }

    return *iterator;
}

bool SongRepository::save_locked(std::string& error) {
    nlohmann::json json = nlohmann::json::array();

    for (const auto& song : songs_) {
        auto item = song.to_json(true);
        item["artistId"] = song.artist_id;
        item["filePath"] = song.file_path;
        json.push_back(std::move(item));
    }

    return util::write_text_file(storage_path_, json.dump(2), error);
}

