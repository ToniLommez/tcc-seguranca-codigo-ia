package repository

import (
	"context"
	"database/sql"
	"fmt"
	"streammusic/internal/models"
	"strings"
)

type SongRepository struct {
	db *sql.DB
}

func NewSongRepository(db *sql.DB) *SongRepository {
	return &SongRepository{db: db}
}

func (r *SongRepository) Create(ctx context.Context, song *models.Song) error {
	query := `
	INSERT INTO songs (id, title, genre, description, file_path, artist_id, artist_name, created_at)
	VALUES (?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := r.db.ExecContext(
		ctx,
		query,
		song.ID,
		song.Title,
		song.Genre,
		song.Description,
		song.FilePath,
		song.ArtistID,
		song.ArtistName,
		song.CreatedAt,
	)

	return err
}

func (r *SongRepository) ListByArtist(ctx context.Context, artistID string) ([]models.Song, error) {
	rows, err := r.db.QueryContext(ctx, `
		SELECT id, title, genre, description, file_path, artist_id, artist_name, created_at
		FROM songs
		WHERE artist_id = ?
		ORDER BY created_at DESC
	`, artistID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	return scanSongs(rows)
}

func (r *SongRepository) List(ctx context.Context, filter models.SongFilter) ([]models.Song, error) {
	query := `
		SELECT id, title, genre, description, file_path, artist_id, artist_name, created_at
		FROM songs
	`

	var args []any
	var conditions []string

	if filter.Title != "" {
		conditions = append(conditions, "LOWER(title) LIKE ?")
		args = append(args, "%"+strings.ToLower(filter.Title)+"%")
	}
	if filter.Artist != "" {
		conditions = append(conditions, "LOWER(artist_name) LIKE ?")
		args = append(args, "%"+strings.ToLower(filter.Artist)+"%")
	}
	if filter.Genre != "" {
		conditions = append(conditions, "LOWER(genre) LIKE ?")
		args = append(args, "%"+strings.ToLower(filter.Genre)+"%")
	}

	if len(conditions) > 0 {
		query += " WHERE " + strings.Join(conditions, " AND ")
	}

	query += " ORDER BY created_at DESC"

	rows, err := r.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	return scanSongs(rows)
}

func (r *SongRepository) GetByID(ctx context.Context, id string) (*models.Song, error) {
	row := r.db.QueryRowContext(ctx, `
		SELECT id, title, genre, description, file_path, artist_id, artist_name, created_at
		FROM songs
		WHERE id = ?
	`, id)

	var song models.Song
	if err := row.Scan(
		&song.ID,
		&song.Title,
		&song.Genre,
		&song.Description,
		&song.FilePath,
		&song.ArtistID,
		&song.ArtistName,
		&song.CreatedAt,
	); err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("song not found")
		}
		return nil, err
	}

	return &song, nil
}

func scanSongs(rows *sql.Rows) ([]models.Song, error) {
	var songs []models.Song

	for rows.Next() {
		var song models.Song
		if err := rows.Scan(
			&song.ID,
			&song.Title,
			&song.Genre,
			&song.Description,
			&song.FilePath,
			&song.ArtistID,
			&song.ArtistName,
			&song.CreatedAt,
		); err != nil {
			return nil, err
		}

		songs = append(songs, song)
	}

	return songs, rows.Err()
}
