package models

import "github.com/golang-jwt/jwt/v5"

type UserType string

const (
	TypeArtist UserType = "ARTISTA"
	TypeUser   UserType = "USUARIO"
)

type User struct {
	ID        string   `json:"id" firestore:"id"`
	Name      string   `json:"name" firestore:"name"`
	Email     string   `json:"email" firestore:"email"`
	Password  string   `json:"-" firestore:"password"`
	Type      UserType `json:"type" firestore:"type"`
	CreatedAt string   `json:"created_at" firestore:"created_at"`
}

type UserPublic struct {
	ID    string   `json:"id"`
	Name  string   `json:"name"`
	Email string   `json:"email"`
	Type  UserType `json:"type"`
}

type MusicDB struct {
	ID          string `json:"id"`
	Title       string `json:"title"`
	Genre       string `json:"genre"`
	Description string `json:"description,omitempty"`
	ArtistID    string `json:"artist_id"`
	ArtistName  string `json:"artist_name"`
	FilePath    string `json:"file_path"`
	CreatedAt   string `json:"created_at"`
}

type Music struct {
	ID          string `json:"id"`
	Title       string `json:"title"`
	Genre       string `json:"genre"`
	Description string `json:"description,omitempty"`
	ArtistID    string `json:"artist_id"`
	ArtistName  string `json:"artist_name"`
	CreatedAt   string `json:"created_at"`
}

func (m *MusicDB) ToPublic() Music {
	return Music{
		ID:          m.ID,
		Title:       m.Title,
		Genre:       m.Genre,
		Description: m.Description,
		ArtistID:    m.ArtistID,
		ArtistName:  m.ArtistName,
		CreatedAt:   m.CreatedAt,
	}
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
	Token string     `json:"token"`
	User  UserPublic `json:"user"`
}

type JWTClaims struct {
	UserID   string   `json:"user_id"`
	UserType UserType `json:"user_type"`
	Name     string   `json:"name"`
	Email    string   `json:"email"`
	jwt.RegisteredClaims
}

type ErrorResponse struct {
	Error string `json:"error"`
}

type SuccessResponse struct {
	Message string `json:"message"`
}
