package models

import "time"

type User struct {
	ID        string    `json:"id" firestore:"-"`
	Name      string    `json:"name" firestore:"name"`
	Email     string    `json:"email" firestore:"email"`
	Password  string    `json:"-" firestore:"password"`
	CreatedAt time.Time `json:"created_at" firestore:"created_at"`
}

type SignupRequest struct {
	Name     string `json:"name"`
	Email    string `json:"email"`
	Password string `json:"password"`
}

type LoginRequest struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

type AuthResponse struct {
	Token string `json:"token"`
	User  User   `json:"user"`
}
