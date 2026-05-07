package models

import "time"

// User representa um gestor/empresa cadastrada no sistema
type User struct {
	ID           string    `json:"id" firestore:"id"`
	Name         string    `json:"name" firestore:"name"`
	Email        string    `json:"email" firestore:"email"`
	PasswordHash string    `json:"-" firestore:"passwordHash"`
	CreatedAt    time.Time `json:"createdAt" firestore:"createdAt"`
}

// RegisterRequest representa o corpo da requisição de cadastro
type RegisterRequest struct {
	Name     string `json:"name" binding:"required,min=2,max=100"`
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=6"`
}

// LoginRequest representa o corpo da requisição de login
type LoginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

// AuthResponse é a resposta retornada após login/cadastro bem-sucedido
type AuthResponse struct {
	Token string `json:"token"`
	User  struct {
		ID    string `json:"id"`
		Name  string `json:"name"`
		Email string `json:"email"`
	} `json:"user"`
}
