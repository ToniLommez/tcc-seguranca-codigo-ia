#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sqlite3.h>

struct SongRecord {
    std::string id;
    std::string title;
    std::string genre;
    std::string description;
    std::string artistId;
    std::string artistName;
    std::string filePath;
    std::string fileName;
    long long   createdAt = 0;
};

class LocalDb {
public:
    explicit LocalDb(const std::string& dbPath);
    ~LocalDb();

    bool initialize();
    bool insertSong(const SongRecord& song);
    std::vector<SongRecord> getAllSongs();
    std::vector<SongRecord> searchSongs(const std::string& title,
                                        const std::string& artist,
                                        const std::string& genre);
    std::optional<SongRecord> getSongById(const std::string& id);
    std::vector<SongRecord> getSongsByArtist(const std::string& artistId);

private:
    sqlite3* m_db = nullptr;
    std::string m_path;
};
