package models

import "time"

type UserType string

const (
	UserTypeArtist UserType = "ARTIST"
	UserTypeUser   UserType = "USER"
)

type User struct {
	ID        string   `json:"id" firestore:"-"`
	Name      string   `json:"name" firestore:"name"`
	Email     string   `json:"email" firestore:"email"`
	Password  string   `json:"-" firestore:"password"`
	Type      UserType `json:"type" firestore:"type"`
	CreatedAt time.Time `json:"created_at" firestore:"created_at"`
}

type RegisterRequest struct {
	Name     string   `json:"name"`
	Email    string   `json:"email"`
	Password string   `json:"password"`
	Type     UserType `json:"type"`
}

type LoginRequest struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

type AuthResponse struct {
	Token string `json:"token"`
	User  User   `json:"user"`
}

type Song struct {
	ID          string    `json:"id" firestore:"-"`
	Title       string    `json:"title" firestore:"title"`
	Genre       string    `json:"genre" firestore:"genre"`
	Description string    `json:"description" firestore:"description"`
	ArtistID    string    `json:"artist_id" firestore:"artist_id"`
	ArtistName  string    `json:"artist_name" firestore:"artist_name"`
	FilePath    string    `json:"-" firestore:"file_path"`
	FileName    string    `json:"file_name" firestore:"file_name"`
	CreatedAt   time.Time `json:"created_at" firestore:"created_at"`
}

type ErrorResponse struct {
	Error string `json:"error"`
}
