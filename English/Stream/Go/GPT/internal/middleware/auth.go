package middleware

import (
	"errors"
	"net/http"
	"streammusic/internal/models"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

const ContextUserKey = "authUser"

type AuthUser struct {
	ID    string
	Name  string
	Email string
	Type  models.UserType
}

type Claims struct {
	UserID   string          `json:"userId"`
	Name     string          `json:"name"`
	Email    string          `json:"email"`
	UserType models.UserType `json:"type"`
	jwt.RegisteredClaims
}

func AuthMiddleware(secret string) gin.HandlerFunc {
	return func(c *gin.Context) {
		tokenString, err := extractToken(c)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": err.Error()})
			c.Abort()
			return
		}

		token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (any, error) {
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, errors.New("unexpected signing method")
			}
			return []byte(secret), nil
		})
		if err != nil || !token.Valid {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid token"})
			c.Abort()
			return
		}

		claims, ok := token.Claims.(*Claims)
		if !ok || claims.ExpiresAt == nil || claims.ExpiresAt.Time.Before(time.Now()) {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "token expired"})
			c.Abort()
			return
		}

		c.Set(ContextUserKey, AuthUser{
			ID:    claims.UserID,
			Name:  claims.Name,
			Email: claims.Email,
			Type:  claims.UserType,
		})
		c.Next()
	}
}

func RequireUserType(userTypes ...models.UserType) gin.HandlerFunc {
	allowed := make(map[models.UserType]struct{}, len(userTypes))
	for _, userType := range userTypes {
		allowed[userType] = struct{}{}
	}

	return func(c *gin.Context) {
		authUser, ok := GetAuthUser(c)
		if !ok {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "authentication required"})
			c.Abort()
			return
		}

		if _, exists := allowed[authUser.Type]; !exists {
			c.JSON(http.StatusForbidden, gin.H{"error": "access denied for this user type"})
			c.Abort()
			return
		}

		c.Next()
	}
}

func GetAuthUser(c *gin.Context) (AuthUser, bool) {
	value, ok := c.Get(ContextUserKey)
	if !ok {
		return AuthUser{}, false
	}

	authUser, ok := value.(AuthUser)
	return authUser, ok
}

func extractToken(c *gin.Context) (string, error) {
	authHeader := c.GetHeader("Authorization")
	if authHeader != "" {
		parts := strings.SplitN(authHeader, " ", 2)
		if len(parts) == 2 && strings.EqualFold(parts[0], "Bearer") && parts[1] != "" {
			return parts[1], nil
		}
	}

	if cookie, err := c.Cookie("auth_token"); err == nil && cookie != "" {
		return cookie, nil
	}

	return "", errors.New("missing authentication token")
}
