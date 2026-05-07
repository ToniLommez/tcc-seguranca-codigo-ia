package middleware

import (
	"context"
	"net/http"
	"strings"

	"github.com/golang-jwt/jwt/v5"

	"musicstream/config"
	"musicstream/models"
)

type contextKey string

const claimsKey contextKey = "claims"

// JWT validates the Bearer token (or ?token= query param for streaming).
func JWT(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		tokenStr := ""

		if auth := r.Header.Get("Authorization"); auth != "" {
			parts := strings.SplitN(auth, " ", 2)
			if len(parts) == 2 && parts[0] == "Bearer" {
				tokenStr = parts[1]
			}
		}

		// Fallback to query param for audio element streaming requests.
		if tokenStr == "" {
			tokenStr = r.URL.Query().Get("token")
		}

		if tokenStr == "" {
			jsonError(w, http.StatusUnauthorized, "unauthorized")
			return
		}

		claims, err := ParseToken(tokenStr)
		if err != nil {
			jsonError(w, http.StatusUnauthorized, "invalid token")
			return
		}

		ctx := context.WithValue(r.Context(), claimsKey, claims)
		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

// RequireRole blocks requests from users that don't match the required type.
func RequireRole(role models.UserType) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			claims := GetClaims(r)
			if claims == nil || claims.UserType != role {
				jsonError(w, http.StatusForbidden, "forbidden: insufficient permissions")
				return
			}
			next.ServeHTTP(w, r)
		})
	}
}

func GetClaims(r *http.Request) *models.JWTClaims {
	claims, _ := r.Context().Value(claimsKey).(*models.JWTClaims)
	return claims
}

func ParseToken(tokenStr string) (*models.JWTClaims, error) {
	claims := &models.JWTClaims{}
	token, err := jwt.ParseWithClaims(tokenStr, claims, func(t *jwt.Token) (interface{}, error) {
		return []byte(config.JWTSecret), nil
	})
	if err != nil || !token.Valid {
		return nil, err
	}
	return claims, nil
}

func jsonError(w http.ResponseWriter, code int, msg string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	w.Write([]byte(`{"error":"` + msg + `"}`))
}
