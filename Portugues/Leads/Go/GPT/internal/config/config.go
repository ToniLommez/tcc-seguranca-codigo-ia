package config

import (
	"os"
	"strconv"
	"strings"
)

type Collections struct {
	Users        string
	Leads        string
	Interactions string
}

type Config struct {
	AppName                 string
	Port                    string
	ProjectID               string
	FirebaseCredentialsPath string
	JWTSecret               string
	JWTIssuer               string
	JWTExpiryHours          int
	CookieName              string
	Collections             Collections
}

func Load() Config {
	return Config{
		AppName:                 getEnv("APP_NAME", "Lead Manager GPT"),
		Port:                    getEnv("PORT", "8080"),
		ProjectID:               getEnv("FIREBASE_PROJECT_ID", "lead-manager-54595"),
		FirebaseCredentialsPath: getEnv("FIREBASE_CREDENTIALS", `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`),
		JWTSecret:               getEnv("JWT_SECRET", "troque-esta-chave-em-producao"),
		JWTIssuer:               getEnv("JWT_ISSUER", "lead-manager-gpt"),
		JWTExpiryHours:          getEnvInt("JWT_EXPIRY_HOURS", 24),
		CookieName:              getEnv("AUTH_COOKIE_NAME", "lead_manager_token"),
		Collections: Collections{
			Users:        getEnv("COLLECTION_USERS", "gpt_go_users"),
			Leads:        getEnv("COLLECTION_LEADS", "gpt_go_leads"),
			Interactions: getEnv("COLLECTION_INTERACTIONS", "gpt_go_interactions"),
		},
	}
}

func getEnv(key, fallback string) string {
	value := strings.TrimSpace(os.Getenv(key))
	if value == "" {
		return fallback
	}
	return value
}

func getEnvInt(key string, fallback int) int {
	value := strings.TrimSpace(os.Getenv(key))
	if value == "" {
		return fallback
	}

	parsed, err := strconv.Atoi(value)
	if err != nil {
		return fallback
	}
	return parsed
}
