package config

import (
	"os"
	"strconv"
	"strings"
	"time"
)

type Config struct {
	AppName                  string
	Port                     string
	JWTSecret                string
	JTTTL                    time.Duration
	FirebaseProjectID        string
	FirebaseCredentialsPath  string
	FirestoreUsersCollection string
	StaticDir                string
	AllowedOrigins           []string
}

func Load() Config {
	return Config{
		AppName:                  getEnv("APP_NAME", "GPT Go Lead Manager"),
		Port:                     getEnv("PORT", "8080"),
		JWTSecret:                getEnv("JWT_SECRET", "change-me-in-production"),
		JTTTL:                    time.Duration(getEnvAsInt("JWT_TTL_HOURS", 24)) * time.Hour,
		FirebaseProjectID:        getEnv("FIREBASE_PROJECT_ID", "lead-manager-54595"),
		FirebaseCredentialsPath:  getEnv("FIREBASE_CREDENTIALS_PATH", "C:/Users/tonil/Desktop/tcc/lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"),
		FirestoreUsersCollection: getEnv("FIRESTORE_USERS_COLLECTION", "gpt5_go_lead_manager_users"),
		StaticDir:                getEnv("STATIC_DIR", "./web"),
		AllowedOrigins:           getEnvAsList("CORS_ALLOWED_ORIGINS", []string{"*"}),
	}
}

func getEnv(key, fallback string) string {
	if value, ok := os.LookupEnv(key); ok && strings.TrimSpace(value) != "" {
		return value
	}
	return fallback
}

func getEnvAsInt(key string, fallback int) int {
	if value, ok := os.LookupEnv(key); ok && strings.TrimSpace(value) != "" {
		if parsed, err := strconv.Atoi(value); err == nil {
			return parsed
		}
	}
	return fallback
}

func getEnvAsList(key string, fallback []string) []string {
	if value, ok := os.LookupEnv(key); ok && strings.TrimSpace(value) != "" {
		parts := strings.Split(value, ",")
		result := make([]string, 0, len(parts))
		for _, part := range parts {
			trimmed := strings.TrimSpace(part)
			if trimmed != "" {
				result = append(result, trimmed)
			}
		}
		if len(result) > 0 {
			return result
		}
	}
	return fallback
}
