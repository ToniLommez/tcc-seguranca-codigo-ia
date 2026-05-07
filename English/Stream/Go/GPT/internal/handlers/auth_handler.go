package handlers

import (
	"net/http"
	"net/mail"
	"strings"
	"time"

	"streammusic/internal/config"
	"streammusic/internal/middleware"
	"streammusic/internal/models"
	"streammusic/internal/repository"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
)

type AuthHandler struct {
	cfg      *config.Config
	userRepo *repository.FirestoreUserRepository
}

type registerRequest struct {
	Name     string `json:"name"`
	Email    string `json:"email"`
	Password string `json:"password"`
	Type     string `json:"type"`
}

type loginRequest struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

func NewAuthHandler(cfg *config.Config, userRepo *repository.FirestoreUserRepository) *AuthHandler {
	return &AuthHandler{cfg: cfg, userRepo: userRepo}
}

func (h *AuthHandler) Register(c *gin.Context) {
	var req registerRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request body"})
		return
	}

	email := strings.TrimSpace(strings.ToLower(req.Email))
	if _, err := mail.ParseAddress(email); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid email address"})
		return
	}

	userType := models.UserType(strings.ToUpper(strings.TrimSpace(req.Type)))
	if userType != models.UserTypeArtist && userType != models.UserTypeUser {
		c.JSON(http.StatusBadRequest, gin.H{"error": "type must be ARTIST or USER"})
		return
	}

	if strings.TrimSpace(req.Name) == "" || len(req.Password) < 6 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "name is required and password must be at least 6 characters"})
		return
	}

	if _, err := h.userRepo.GetByEmail(c.Request.Context(), email); err == nil {
		c.JSON(http.StatusConflict, gin.H{"error": "email is already registered"})
		return
	}

	passwordHash, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to hash password"})
		return
	}

	user := &models.User{
		ID:           uuid.NewString(),
		Name:         strings.TrimSpace(req.Name),
		Email:        email,
		PasswordHash: string(passwordHash),
		Type:         userType,
		CreatedAt:    time.Now().UTC(),
	}

	if err := h.userRepo.Create(c.Request.Context(), user); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create user"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message": "user created successfully",
		"user":    sanitizeUser(user),
	})
}

func (h *AuthHandler) Login(c *gin.Context) {
	var req loginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request body"})
		return
	}

	email := strings.TrimSpace(strings.ToLower(req.Email))
	user, err := h.userRepo.GetByEmail(c.Request.Context(), email)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid email or password"})
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(req.Password)); err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid email or password"})
		return
	}

	token, err := h.createToken(user)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create token"})
		return
	}

	h.setTokenCookie(c, token)
	c.JSON(http.StatusOK, gin.H{
		"token": token,
		"user":  sanitizeUser(user),
	})
}

func (h *AuthHandler) Logout(c *gin.Context) {
	c.SetCookie("auth_token", "", -1, "/", "", false, true)
	c.JSON(http.StatusOK, gin.H{"message": "logged out successfully"})
}

func (h *AuthHandler) Me(c *gin.Context) {
	authUser, ok := middleware.GetAuthUser(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "authentication required"})
		return
	}

	user, err := h.userRepo.GetByID(c.Request.Context(), authUser.ID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"user": sanitizeUser(user)})
}

func (h *AuthHandler) createToken(user *models.User) (string, error) {
	claims := middleware.Claims{
		UserID:   user.ID,
		Name:     user.Name,
		Email:    user.Email,
		UserType: user.Type,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   user.ID,
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(h.cfg.TokenDuration)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(h.cfg.JWTSecret))
}

func (h *AuthHandler) setTokenCookie(c *gin.Context, token string) {
	maxAge := int(h.cfg.TokenDuration.Seconds())
	c.SetCookie("auth_token", token, maxAge, "/", "", false, true)
}

func sanitizeUser(user *models.User) gin.H {
	return gin.H{
		"id":        user.ID,
		"name":      user.Name,
		"email":     user.Email,
		"type":      user.Type,
		"createdAt": user.CreatedAt,
	}
}
