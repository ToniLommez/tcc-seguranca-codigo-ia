package domain

import (
	"fmt"
	"strings"
	"time"
)

type UserType string

const (
	UserTypeArtist UserType = "ARTISTA"
	UserTypeUser   UserType = "USUARIO"
)

type User struct {
	ID           string    `firestore:"id" json:"id"`
	Name         string    `firestore:"name" json:"name"`
	Email        string    `firestore:"email" json:"email"`
	PasswordHash string    `firestore:"passwordHash" json:"-"`
	Type         UserType  `firestore:"type" json:"type"`
	CreatedAt    time.Time `firestore:"createdAt" json:"createdAt"`
}

type Song struct {
	ID           int64     `json:"id"`
	Title        string    `json:"title"`
	Genre        string    `json:"genre"`
	Description  string    `json:"description,omitempty"`
	ArtistEmail  string    `json:"artistEmail"`
	ArtistName   string    `json:"artistName"`
	FilePath     string    `json:"-"`
	OriginalName string    `json:"originalName"`
	CreatedAt    time.Time `json:"createdAt"`
}

func NormalizeUserType(raw string) (UserType, error) {
	switch UserType(strings.ToUpper(strings.TrimSpace(raw))) {
	case UserTypeArtist:
		return UserTypeArtist, nil
	case UserTypeUser:
		return UserTypeUser, nil
	default:
		return "", fmt.Errorf("invalid user type")
	}
}
