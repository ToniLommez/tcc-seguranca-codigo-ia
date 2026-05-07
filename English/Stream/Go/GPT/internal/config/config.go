package config

import (
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"time"
)

const (
	defaultFirebaseCredentials = `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`
	defaultJWTSecret           = "replace-this-secret-in-production"
)

type firebaseServiceAccount struct {
	ProjectID string `json:"project_id"`
}

type Config struct {
	Port                string
	JWTSecret           string
	TokenDuration       time.Duration
	FirebaseCredentials string
	FirebaseProjectID   string
	UploadDir           string
	SongsDBPath         string
}

func Load() (*Config, error) {
	credentialsPath := envOrDefault("FIREBASE_CREDENTIALS", defaultFirebaseCredentials)
	projectID := os.Getenv("FIREBASE_PROJECT_ID")

	if projectID == "" {
		var err error
		projectID, err = resolveProjectID(credentialsPath)
		if err != nil {
			return nil, err
		}
	}

	tokenDuration, err := time.ParseDuration(envOrDefault("JWT_DURATION", "24h"))
	if err != nil {
		return nil, err
	}

	cfg := &Config{
		Port:                envOrDefault("PORT", "8080"),
		JWTSecret:           envOrDefault("JWT_SECRET", defaultJWTSecret),
		TokenDuration:       tokenDuration,
		FirebaseCredentials: credentialsPath,
		FirebaseProjectID:   projectID,
		UploadDir:           envOrDefault("UPLOAD_DIR", filepath.Join(".", "uploads")),
		SongsDBPath:         envOrDefault("SONGS_DB_PATH", filepath.Join(".", "data", "songs.db")),
	}

	if cfg.JWTSecret == "" {
		return nil, errors.New("JWT_SECRET cannot be empty")
	}

	return cfg, nil
}

func resolveProjectID(credentialsPath string) (string, error) {
	raw, err := os.ReadFile(credentialsPath)
	if err != nil {
		return "", err
	}

	var svc firebaseServiceAccount
	if err := json.Unmarshal(raw, &svc); err != nil {
		return "", err
	}

	if svc.ProjectID == "" {
		return "", errors.New("project_id missing from Firebase credentials")
	}

	return svc.ProjectID, nil
}

func envOrDefault(key, fallback string) string {
	value := os.Getenv(key)
	if value == "" {
		return fallback
	}
	return value
}
