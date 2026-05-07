package db

import (
	"encoding/json"
	"os"
	"strings"
	"sync"
	"time"

	"musicstream/models"
)

type Database struct {
	path   string
	mu     sync.RWMutex
	musics []models.MusicDB
}

func InitSQLite() (*Database, error) {
	path := os.Getenv("DB_PATH")
	if path == "" {
		path = "musicstream.json"
	}

	d := &Database{path: path}
	if err := d.load(); err != nil {
		return nil, err
	}
	return d, nil
}

func (d *Database) Close() error { return nil }

func (d *Database) load() error {
	d.mu.Lock()
	defer d.mu.Unlock()

	data, err := os.ReadFile(d.path)
	if os.IsNotExist(err) {
		d.musics = []models.MusicDB{}
		return nil
	}
	if err != nil {
		return err
	}
	return json.Unmarshal(data, &d.musics)
}

func (d *Database) persist() error {
	data, err := json.MarshalIndent(d.musics, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(d.path, data, 0644)
}

func (d *Database) CreateMusic(m *models.MusicDB) error {
	d.mu.Lock()
	defer d.mu.Unlock()

	m.CreatedAt = time.Now().UTC().Format(time.RFC3339)
	d.musics = append(d.musics, *m)
	return d.persist()
}

func (d *Database) GetMusicByID(id string) (*models.MusicDB, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	for i := range d.musics {
		if d.musics[i].ID == id {
			m := d.musics[i]
			return &m, nil
		}
	}
	return nil, os.ErrNotExist
}

func (d *Database) ListAllMusic() ([]models.MusicDB, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	result := make([]models.MusicDB, len(d.musics))
	copy(result, d.musics)
	return result, nil
}

func (d *Database) ListMusicByArtist(artistID string) ([]models.MusicDB, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	result := make([]models.MusicDB, 0)
	for _, m := range d.musics {
		if m.ArtistID == artistID {
			result = append(result, m)
		}
	}
	return result, nil
}

func (d *Database) SearchMusic(title, artistName, genre string) ([]models.MusicDB, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	result := make([]models.MusicDB, 0)
	for _, m := range d.musics {
		if title != "" && !strings.Contains(strings.ToLower(m.Title), strings.ToLower(title)) {
			continue
		}
		if artistName != "" && !strings.Contains(strings.ToLower(m.ArtistName), strings.ToLower(artistName)) {
			continue
		}
		if genre != "" && !strings.Contains(strings.ToLower(m.Genre), strings.ToLower(genre)) {
			continue
		}
		result = append(result, m)
	}
	return result, nil
}
