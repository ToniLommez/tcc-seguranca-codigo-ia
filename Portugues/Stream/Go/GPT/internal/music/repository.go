package music

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	_ "modernc.org/sqlite"

	"streamapp/internal/domain"
)

type Repository struct {
	db *sql.DB
}

func NewRepository(path string) (*Repository, error) {
	db, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, fmt.Errorf("open sqlite: %w", err)
	}

	repo := &Repository{db: db}
	if err := repo.migrate(); err != nil {
		return nil, err
	}

	return repo, nil
}

func (r *Repository) Close() error {
	return r.db.Close()
}

func (r *Repository) migrate() error {
	query := `
	CREATE TABLE IF NOT EXISTS songs (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		title TEXT NOT NULL,
		genre TEXT NOT NULL,
		description TEXT,
		artist_email TEXT NOT NULL,
		artist_name TEXT NOT NULL,
		file_path TEXT NOT NULL,
		original_name TEXT NOT NULL,
		created_at TEXT NOT NULL
	);
	`

	if _, err := r.db.Exec(query); err != nil {
		return fmt.Errorf("migrate songs table: %w", err)
	}

	return nil
}

func (r *Repository) Create(song domain.Song) (domain.Song, error) {
	now := time.Now().UTC()
	result, err := r.db.Exec(
		`INSERT INTO songs (title, genre, description, artist_email, artist_name, file_path, original_name, created_at)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
		song.Title,
		song.Genre,
		song.Description,
		song.ArtistEmail,
		song.ArtistName,
		song.FilePath,
		song.OriginalName,
		now.Format(time.RFC3339),
	)
	if err != nil {
		return domain.Song{}, fmt.Errorf("insert song: %w", err)
	}

	id, err := result.LastInsertId()
	if err != nil {
		return domain.Song{}, fmt.Errorf("fetch inserted id: %w", err)
	}

	song.ID = id
	song.CreatedAt = now
	return song, nil
}

func (r *Repository) ListByArtist(email string) ([]domain.Song, error) {
	rows, err := r.db.Query(
		`SELECT id, title, genre, description, artist_email, artist_name, file_path, original_name, created_at
		 FROM songs
		 WHERE artist_email = ?
		 ORDER BY created_at DESC`,
		strings.ToLower(strings.TrimSpace(email)),
	)
	if err != nil {
		return nil, fmt.Errorf("query artist songs: %w", err)
	}
	defer rows.Close()

	return scanSongs(rows)
}

func (r *Repository) Search(title, artist, genre string) ([]domain.Song, error) {
	rows, err := r.db.Query(
		`SELECT id, title, genre, description, artist_email, artist_name, file_path, original_name, created_at
		 FROM songs
		 WHERE lower(title) LIKE '%' || lower(?) || '%'
		   AND lower(artist_name) LIKE '%' || lower(?) || '%'
		   AND lower(genre) LIKE '%' || lower(?) || '%'
		 ORDER BY created_at DESC`,
		strings.TrimSpace(title),
		strings.TrimSpace(artist),
		strings.TrimSpace(genre),
	)
	if err != nil {
		return nil, fmt.Errorf("search songs: %w", err)
	}
	defer rows.Close()

	return scanSongs(rows)
}

func (r *Repository) GetByID(id int64) (domain.Song, error) {
	row := r.db.QueryRow(
		`SELECT id, title, genre, description, artist_email, artist_name, file_path, original_name, created_at
		 FROM songs WHERE id = ?`,
		id,
	)

	var (
		song      domain.Song
		createdAt string
	)
	if err := row.Scan(
		&song.ID,
		&song.Title,
		&song.Genre,
		&song.Description,
		&song.ArtistEmail,
		&song.ArtistName,
		&song.FilePath,
		&song.OriginalName,
		&createdAt,
	); err != nil {
		if err == sql.ErrNoRows {
			return domain.Song{}, fmt.Errorf("song not found")
		}
		return domain.Song{}, fmt.Errorf("get song: %w", err)
	}

	parsed, err := time.Parse(time.RFC3339, createdAt)
	if err != nil {
		return domain.Song{}, fmt.Errorf("parse created at: %w", err)
	}
	song.CreatedAt = parsed

	return song, nil
}

func scanSongs(rows *sql.Rows) ([]domain.Song, error) {
	var songs []domain.Song

	for rows.Next() {
		var (
			song      domain.Song
			createdAt string
		)
		if err := rows.Scan(
			&song.ID,
			&song.Title,
			&song.Genre,
			&song.Description,
			&song.ArtistEmail,
			&song.ArtistName,
			&song.FilePath,
			&song.OriginalName,
			&createdAt,
		); err != nil {
			return nil, fmt.Errorf("scan song: %w", err)
		}

		parsed, err := time.Parse(time.RFC3339, createdAt)
		if err != nil {
			return nil, fmt.Errorf("parse created at: %w", err)
		}
		song.CreatedAt = parsed
		songs = append(songs, song)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate songs: %w", err)
	}

	return songs, nil
}
