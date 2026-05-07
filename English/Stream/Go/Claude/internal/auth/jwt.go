package auth

import (
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"

	"music-stream/internal/models"
)

type Claims struct {
	UserID   string          `json:"user_id"`
	Email    string          `json:"email"`
	UserType models.UserType `json:"user_type"`
	jwt.RegisteredClaims
}

func GenerateToken(user *models.User, secret string) (string, error) {
	claims := Claims{
		UserID:   user.ID,
		Email:    user.Email,
		UserType: user.Type,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(24 * time.Hour)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(secret))
}

func ValidateToken(tokenString, secret string) (*Claims, error) {
	token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(secret), nil
	})
	if err != nil {
		return nil, err
	}

	claims, ok := token.Claims.(*Claims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("invalid token")
	}

	return claims, nil
}
