package service

import (
	"context"
	"errors"
	"fmt"
	"net/mail"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/crypto/bcrypt"

	"leadmanagergpt/internal/config"
	"leadmanagergpt/internal/models"
	"leadmanagergpt/internal/store"
)

var (
	ErrInvalidCredentials = errors.New("credenciais invalidas")
	ErrEmailInUse         = errors.New("email ja cadastrado")
)

type JWTClaims struct {
	UserID string `json:"user_id"`
	Email  string `json:"email"`
	jwt.RegisteredClaims
}

type AuthService struct {
	store  *store.FirestoreStore
	config config.Config
}

func NewAuthService(store *store.FirestoreStore, cfg config.Config) *AuthService {
	return &AuthService{
		store:  store,
		config: cfg,
	}
}

func (s *AuthService) Register(ctx context.Context, name, email, password string) (models.User, string, error) {
	name = strings.TrimSpace(name)
	email = strings.TrimSpace(email)
	password = strings.TrimSpace(password)

	if name == "" || email == "" || password == "" {
		return models.User{}, "", errors.New("nome, email e senha sao obrigatorios")
	}
	if len(password) < 6 {
		return models.User{}, "", errors.New("a senha deve ter pelo menos 6 caracteres")
	}
	if _, err := mail.ParseAddress(email); err != nil {
		return models.User{}, "", errors.New("email invalido")
	}

	_, err := s.store.GetUserByEmail(ctx, email)
	if err == nil {
		return models.User{}, "", ErrEmailInUse
	}
	if err != nil && !errors.Is(err, store.ErrNotFound) {
		return models.User{}, "", err
	}

	passwordHash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return models.User{}, "", fmt.Errorf("generate password hash: %w", err)
	}

	user, err := s.store.CreateUser(ctx, models.User{
		Name:         name,
		Email:        email,
		PasswordHash: string(passwordHash),
	})
	if err != nil {
		return models.User{}, "", err
	}

	token, err := s.GenerateToken(user)
	if err != nil {
		return models.User{}, "", err
	}

	return user, token, nil
}

func (s *AuthService) Login(ctx context.Context, email, password string) (models.User, string, error) {
	user, err := s.store.GetUserByEmail(ctx, email)
	if err != nil {
		if errors.Is(err, store.ErrNotFound) {
			return models.User{}, "", ErrInvalidCredentials
		}
		return models.User{}, "", err
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(password)); err != nil {
		return models.User{}, "", ErrInvalidCredentials
	}

	token, err := s.GenerateToken(user)
	if err != nil {
		return models.User{}, "", err
	}

	return user, token, nil
}

func (s *AuthService) GenerateToken(user models.User) (string, error) {
	now := time.Now()
	claims := JWTClaims{
		UserID: user.ID,
		Email:  user.Email,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    s.config.JWTIssuer,
			Subject:   user.ID,
			ExpiresAt: jwt.NewNumericDate(now.Add(time.Duration(s.config.JWTExpiryHours) * time.Hour)),
			IssuedAt:  jwt.NewNumericDate(now),
			NotBefore: jwt.NewNumericDate(now),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	signed, err := token.SignedString([]byte(s.config.JWTSecret))
	if err != nil {
		return "", fmt.Errorf("sign token: %w", err)
	}
	return signed, nil
}

func (s *AuthService) ParseToken(token string) (*JWTClaims, error) {
	parsedToken, err := jwt.ParseWithClaims(token, &JWTClaims{}, func(t *jwt.Token) (interface{}, error) {
		return []byte(s.config.JWTSecret), nil
	})
	if err != nil {
		return nil, ErrInvalidCredentials
	}

	claims, ok := parsedToken.Claims.(*JWTClaims)
	if !ok || !parsedToken.Valid {
		return nil, ErrInvalidCredentials
	}

	return claims, nil
}

func (s *AuthService) GetUserByID(ctx context.Context, userID string) (models.User, error) {
	return s.store.GetUserByID(ctx, userID)
}
