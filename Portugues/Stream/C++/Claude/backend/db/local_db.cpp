#include "local_db.h"
#include <stdexcept>
#include <iostream>

static SongRecord rowToSong(sqlite3_stmt* stmt) {
    SongRecord s;
    s.id          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    s.title       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    s.genre       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    auto desc     = sqlite3_column_text(stmt, 3);
    s.description = desc ? reinterpret_cast<const char*>(desc) : "";
    s.artistId    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    s.artistName  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    s.filePath    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    s.fileName    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    s.createdAt   = sqlite3_column_int64(stmt, 8);
    return s;
}

LocalDb::LocalDb(const std::string& dbPath) : m_path(dbPath) {}

LocalDb::~LocalDb() {
    if (m_db) sqlite3_close(m_db);
}

bool LocalDb::initialize() {
    if (sqlite3_open(m_path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "[DB] Cannot open database: " << sqlite3_errmsg(m_db) << "\n";
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS songs ("
        "  id          TEXT PRIMARY KEY,"
        "  title       TEXT NOT NULL,"
        "  genre       TEXT NOT NULL,"
        "  description TEXT,"
        "  artist_id   TEXT NOT NULL,"
        "  artist_name TEXT NOT NULL,"
        "  file_path   TEXT NOT NULL,"
        "  file_name   TEXT NOT NULL,"
        "  created_at  INTEGER NOT NULL"
        ");";

    char* err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] Init error: " << err << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool LocalDb::insertSong(const SongRecord& s) {
    const char* sql =
        "INSERT INTO songs (id, title, genre, description, artist_id, artist_name, "
        "                   file_path, file_name, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, s.id.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.title.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.genre.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, s.description.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, s.artistId.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, s.artistName.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, s.filePath.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, s.fileName.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, s.createdAt);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<SongRecord> LocalDb::getAllSongs() {
    const char* sql =
        "SELECT id, title, genre, description, artist_id, artist_name, "
        "       file_path, file_name, created_at "
        "FROM songs ORDER BY created_at DESC;";

    sqlite3_stmt* stmt;
    std::vector<SongRecord> rows;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return rows;

    while (sqlite3_step(stmt) == SQLITE_ROW) rows.push_back(rowToSong(stmt));
    sqlite3_finalize(stmt);
    return rows;
}

std::vector<SongRecord> LocalDb::searchSongs(const std::string& title,
                                              const std::string& artist,
                                              const std::string& genre) {
    std::string sql =
        "SELECT id, title, genre, description, artist_id, artist_name, "
        "       file_path, file_name, created_at "
        "FROM songs WHERE 1=1";

    std::vector<std::string> params;
    if (!title.empty())  { sql += " AND LOWER(title) LIKE LOWER(?)";       params.push_back("%" + title + "%"); }
    if (!artist.empty()) { sql += " AND LOWER(artist_name) LIKE LOWER(?)"; params.push_back("%" + artist + "%"); }
    if (!genre.empty())  { sql += " AND LOWER(genre) LIKE LOWER(?)";       params.push_back("%" + genre + "%"); }
    sql += " ORDER BY created_at DESC;";

    sqlite3_stmt* stmt;
    std::vector<SongRecord> rows;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return rows;

    for (int i = 0; i < (int)params.size(); ++i)
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) rows.push_back(rowToSong(stmt));
    sqlite3_finalize(stmt);
    return rows;
}

std::optional<SongRecord> LocalDb::getSongById(const std::string& id) {
    const char* sql =
        "SELECT id, title, genre, description, artist_id, artist_name, "
        "       file_path, file_name, created_at "
        "FROM songs WHERE id = ?;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<SongRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToSong(stmt);
    sqlite3_finalize(stmt);
    return result;
}

std::vector<SongRecord> LocalDb::getSongsByArtist(const std::string& artistId) {
    const char* sql =
        "SELECT id, title, genre, description, artist_id, artist_name, "
        "       file_path, file_name, created_at "
        "FROM songs WHERE artist_id = ? ORDER BY created_at DESC;";

    sqlite3_stmt* stmt;
    std::vector<SongRecord> rows;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return rows;
    sqlite3_bind_text(stmt, 1, artistId.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) rows.push_back(rowToSong(stmt));
    sqlite3_finalize(stmt);
    return rows;
}
