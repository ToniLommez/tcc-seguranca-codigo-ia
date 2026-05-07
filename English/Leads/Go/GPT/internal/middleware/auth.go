package middleware

import (
	"context"
	"net/http"
	"strings"

	"leadmanager/internal/service"
)

type contextKey string

const userContextKey contextKey = "auth-user-id"

func RequireAuth(authService *service.AuthService) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			header := strings.TrimSpace(r.Header.Get("Authorization"))
			if !strings.HasPrefix(strings.ToLower(header), "bearer ") {
				http.Error(w, "missing bearer token", http.StatusUnauthorized)
				return
			}

			token := strings.TrimSpace(header[7:])
			claims, err := authService.ValidateToken(token)
			if err != nil {
				http.Error(w, "invalid token", http.StatusUnauthorized)
				return
			}

			ctx := context.WithValue(r.Context(), userContextKey, claims.UserID)
			next.ServeHTTP(w, r.WithContext(ctx))
		})
	}
}

func UserIDFromContext(ctx context.Context) string {
	value, _ := ctx.Value(userContextKey).(string)
	return value
}
