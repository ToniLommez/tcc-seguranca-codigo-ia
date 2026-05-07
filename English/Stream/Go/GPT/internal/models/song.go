package models

import "time"

type Song struct {
	ID          string    `json:"id"`
	Title       string    `json:"title"`
	Genre       string    `json:"genre"`
	Description string    `json:"description,omitempty"`
	FilePath    string    `json:"-"`
	ArtistID    string    `json:"artistId"`
	ArtistName  string    `json:"artistName"`
	CreatedAt   time.Time `json:"createdAt"`
}

type SongFilter struct {
	Title  string
	Artist string
	Genre  string
}
