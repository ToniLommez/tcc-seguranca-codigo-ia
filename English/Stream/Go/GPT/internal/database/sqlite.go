package database

import (
	"database/sql"
	"os"
	"path/filepath"

	_ "modernc.org/sqlite"
)

func OpenSQLite(dbPath string) (*sql.DB, error) {
	if err := os.MkdirAll(filepath.Dir(dbPath), 0o755); err != nil {
		return nil, err
	}

	db, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, err
	}

	schema := `
	CREATE TABLE IF NOT EXISTS songs (
		id TEXT PRIMARY KEY,
		title TEXT NOT NULL,
		genre TEXT NOT NULL,
		description TEXT,
		file_path TEXT NOT NULL,
		artist_id TEXT NOT NULL,
		artist_name TEXT NOT NULL,
		created_at DATETIME NOT NULL
	);

	CREATE INDEX IF NOT EXISTS idx_songs_artist_id ON songs (artist_id);
	CREATE INDEX IF NOT EXISTS idx_songs_title ON songs (title);
	CREATE INDEX IF NOT EXISTS idx_songs_genre ON songs (genre);
	`

	if _, err := db.Exec(schema); err != nil {
		db.Close()
		return nil, err
	}

	return db, nil
}
