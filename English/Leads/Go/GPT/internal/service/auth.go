package service

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"

	"leadmanager/internal/config"
	"leadmanager/internal/models"
	"leadmanager/internal/repository"
)

var ErrInvalidCredentials = errors.New("invalid email or password")

type AuthService struct {
	cfg  config.Config
	repo *repository.FirestoreRepository
}

type AuthTokenClaims struct {
	UserID string `json:"userId"`
	Email  string `json:"email"`
	jwt.RegisteredClaims
}

func NewAuthService(cfg config.Config, repo *repository.FirestoreRepository) *AuthService {
	return &AuthService{cfg: cfg, repo: repo}
}

func (s *AuthService) Register(ctx context.Context, name, email, password string) (models.User, string, error) {
	name = strings.TrimSpace(name)
	email = normalizeEmail(email)
	password = strings.TrimSpace(password)

	if name == "" || email == "" || password == "" {
		return models.User{}, "", errors.New("name, email and password are required")
	}
	if len(password) < 6 {
		return models.User{}, "", errors.New("password must have at least 6 characters")
	}

	if _, err := s.repo.GetUserByEmail(ctx, email); err == nil {
		return models.User{}, "", errors.New("email is already registered")
	} else if !errors.Is(err, repository.ErrNotFound) {
		return models.User{}, "", err
	}

	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return models.User{}, "", fmt.Errorf("hash password: %w", err)
	}

	user := models.User{
		ID:           uuid.NewString(),
		Name:         name,
		Email:        email,
		PasswordHash: string(hash),
		CreatedAt:    time.Now().UTC(),
	}

	if err := s.repo.CreateUser(ctx, user); err != nil {
		return models.User{}, "", err
	}

	token, err := s.issueToken(user)
	if err != nil {
		return models.User{}, "", err
	}

	user.PasswordHash = ""
	return user, token, nil
}

func (s *AuthService) Login(ctx context.Context, email, password string) (models.User, string, error) {
	user, err := s.repo.GetUserByEmail(ctx, normalizeEmail(email))
	if err != nil {
		if errors.Is(err, repository.ErrNotFound) {
			return models.User{}, "", ErrInvalidCredentials
		}
		return models.User{}, "", err
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(password)); err != nil {
		return models.User{}, "", ErrInvalidCredentials
	}

	token, err := s.issueToken(user)
	if err != nil {
		return models.User{}, "", err
	}

	user.PasswordHash = ""
	return user, token, nil
}

func (s *AuthService) ValidateToken(tokenString string) (*AuthTokenClaims, error) {
	token, err := jwt.ParseWithClaims(tokenString, &AuthTokenClaims{}, func(token *jwt.Token) (interface{}, error) {
		return []byte(s.cfg.JWTSecret), nil
	})
	if err != nil {
		return nil, err
	}

	claims, ok := token.Claims.(*AuthTokenClaims)
	if !ok || !token.Valid {
		return nil, errors.New("invalid token")
	}

	return claims, nil
}

func (s *AuthService) GetUser(ctx context.Context, userID string) (models.User, error) {
	user, err := s.repo.GetUserByID(ctx, userID)
	if err != nil {
		return models.User{}, err
	}
	user.PasswordHash = ""
	return user, nil
}

func (s *AuthService) issueToken(user models.User) (string, error) {
	now := time.Now().UTC()
	claims := AuthTokenClaims{
		UserID: user.ID,
		Email:  user.Email,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   user.ID,
			ExpiresAt: jwt.NewNumericDate(now.Add(s.cfg.JTTTL)),
			IssuedAt:  jwt.NewNumericDate(now),
			NotBefore: jwt.NewNumericDate(now),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(s.cfg.JWTSecret))
}

func normalizeEmail(email string) string {
	return strings.ToLower(strings.TrimSpace(email))
}
