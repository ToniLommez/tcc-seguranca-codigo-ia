package handlers

import (
	"encoding/json"
	"net/http"
	"strings"
	"time"

	"golang.org/x/crypto/bcrypt"

	"music-stream/internal/auth"
	"music-stream/internal/config"
	"music-stream/internal/firebase"
	"music-stream/internal/models"
)

type AuthHandler struct {
	fb  *firebase.Client
	cfg *config.Config
}

func NewAuthHandler(fb *firebase.Client, cfg *config.Config) *AuthHandler {
	return &AuthHandler{fb: fb, cfg: cfg}
}

func (h *AuthHandler) Register(w http.ResponseWriter, r *http.Request) {
	var req models.RegisterRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		respondError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	if req.Name == "" || req.Email == "" || req.Password == "" {
		respondError(w, http.StatusBadRequest, "name, email, and password are required")
		return
	}

	req.Email = strings.ToLower(strings.TrimSpace(req.Email))

	if req.Type != models.UserTypeArtist && req.Type != models.UserTypeUser {
		respondError(w, http.StatusBadRequest, "type must be ARTIST or USER")
		return
	}

	if len(req.Password) < 6 {
		respondError(w, http.StatusBadRequest, "password must be at least 6 characters")
		return
	}

	existing, _ := h.fb.GetUserByEmail(r.Context(), req.Email)
	if existing != nil {
		respondError(w, http.StatusConflict, "email already registered")
		return
	}

	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to process password")
		return
	}

	user := &models.User{
		Name:      req.Name,
		Email:     req.Email,
		Password:  string(hashedPassword),
		Type:      req.Type,
		CreatedAt: time.Now(),
	}

	id, err := h.fb.CreateUser(r.Context(), user)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to create user")
		return
	}
	user.ID = id

	token, err := auth.GenerateToken(user, h.cfg.JWTSecret)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to generate token")
		return
	}

	respondJSON(w, http.StatusCreated, models.AuthResponse{
		Token: token,
		User:  *user,
	})
}

func (h *AuthHandler) Login(w http.ResponseWriter, r *http.Request) {
	var req models.LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		respondError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	if req.Email == "" || req.Password == "" {
		respondError(w, http.StatusBadRequest, "email and password are required")
		return
	}

	req.Email = strings.ToLower(strings.TrimSpace(req.Email))

	user, err := h.fb.GetUserByEmail(r.Context(), req.Email)
	if err != nil {
		respondError(w, http.StatusUnauthorized, "invalid email or password")
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		respondError(w, http.StatusUnauthorized, "invalid email or password")
		return
	}

	token, err := auth.GenerateToken(user, h.cfg.JWTSecret)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to generate token")
		return
	}

	respondJSON(w, http.StatusOK, models.AuthResponse{
		Token: token,
		User:  *user,
	})
}

func respondJSON(w http.ResponseWriter, status int, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(data)
}

func respondError(w http.ResponseWriter, status int, message string) {
	respondJSON(w, status, models.ErrorResponse{Error: message})
}
