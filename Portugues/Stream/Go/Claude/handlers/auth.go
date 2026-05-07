package handlers

import (
	"context"
	"encoding/json"
	"net/http"
	"time"

	firebase "firebase.google.com/go/v4"
	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
	"google.golang.org/api/iterator"

	"musicstream/config"
	"musicstream/models"
)

type AuthHandler struct {
	app *firebase.App
}

func NewAuthHandler(app *firebase.App) *AuthHandler {
	return &AuthHandler{app: app}
}

func (h *AuthHandler) Register(w http.ResponseWriter, r *http.Request) {
	var req models.RegisterRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "corpo da requisição inválido"})
		return
	}

	if req.Name == "" || req.Email == "" || req.Password == "" {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "nome, e-mail e senha são obrigatórios"})
		return
	}
	if req.Type != models.TypeArtist && req.Type != models.TypeUser {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "tipo deve ser ARTISTA ou USUARIO"})
		return
	}

	ctx := context.Background()
	client, err := h.app.Firestore(ctx)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao conectar ao banco de dados"})
		return
	}
	defer client.Close()

	iter := client.Collection("users").Where("email", "==", req.Email).Documents(ctx)
	defer iter.Stop()
	_, err = iter.Next()
	if err == nil {
		writeJSON(w, http.StatusConflict, models.ErrorResponse{Error: "e-mail já cadastrado"})
		return
	}
	if err != iterator.Done {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao verificar e-mail"})
		return
	}

	hashed, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro interno"})
		return
	}

	userID := uuid.New().String()
	user := models.User{
		ID:        userID,
		Name:      req.Name,
		Email:     req.Email,
		Password:  string(hashed),
		Type:      req.Type,
		CreatedAt: time.Now().UTC().Format(time.RFC3339),
	}

	if _, err = client.Collection("users").Doc(userID).Set(ctx, user); err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao criar usuário"})
		return
	}

	token, err := generateToken(user)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao gerar token"})
		return
	}

	writeJSON(w, http.StatusCreated, models.AuthResponse{
		Token: token,
		User:  models.UserPublic{ID: user.ID, Name: user.Name, Email: user.Email, Type: user.Type},
	})
}

func (h *AuthHandler) Login(w http.ResponseWriter, r *http.Request) {
	var req models.LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "corpo da requisição inválido"})
		return
	}

	ctx := context.Background()
	client, err := h.app.Firestore(ctx)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao conectar ao banco de dados"})
		return
	}
	defer client.Close()

	iter := client.Collection("users").Where("email", "==", req.Email).Documents(ctx)
	defer iter.Stop()
	doc, err := iter.Next()
	if err == iterator.Done {
		writeJSON(w, http.StatusUnauthorized, models.ErrorResponse{Error: "credenciais inválidas"})
		return
	}
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao buscar usuário"})
		return
	}

	var user models.User
	if err := doc.DataTo(&user); err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro interno"})
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		writeJSON(w, http.StatusUnauthorized, models.ErrorResponse{Error: "credenciais inválidas"})
		return
	}

	token, err := generateToken(user)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao gerar token"})
		return
	}

	writeJSON(w, http.StatusOK, models.AuthResponse{
		Token: token,
		User:  models.UserPublic{ID: user.ID, Name: user.Name, Email: user.Email, Type: user.Type},
	})
}

func generateToken(user models.User) (string, error) {
	claims := models.JWTClaims{
		UserID:   user.ID,
		UserType: user.Type,
		Name:     user.Name,
		Email:    user.Email,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(24 * time.Hour)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(config.JWTSecret))
}

func writeJSON(w http.ResponseWriter, status int, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}
