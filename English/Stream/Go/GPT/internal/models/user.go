package models

import "time"

type UserType string

const (
	UserTypeArtist UserType = "ARTIST"
	UserTypeUser   UserType = "USER"
)

type User struct {
	ID           string    `json:"id" firestore:"id"`
	Name         string    `json:"name" firestore:"name"`
	Email        string    `json:"email" firestore:"email"`
	PasswordHash string    `json:"-" firestore:"passwordHash"`
	Type         UserType  `json:"type" firestore:"type"`
	CreatedAt    time.Time `json:"createdAt" firestore:"createdAt"`
}
