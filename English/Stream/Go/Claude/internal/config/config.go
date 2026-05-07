package config

import (
	"os"
	"path/filepath"
)

type Config struct {
	Port            string
	JWTSecret       string
	FirebaseCredFile string
	UploadsDir      string
}

func Load() *Config {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	jwtSecret := os.Getenv("JWT_SECRET")
	if jwtSecret == "" {
		jwtSecret = "music-stream-secret-key-change-in-production"
	}

	credFile := os.Getenv("FIREBASE_CREDENTIALS")
	if credFile == "" {
		credFile = "firebase-credentials.json"
	}

	uploadsDir := os.Getenv("UPLOADS_DIR")
	if uploadsDir == "" {
		uploadsDir = "uploads"
	}

	absUploads, err := filepath.Abs(uploadsDir)
	if err == nil {
		uploadsDir = absUploads
	}

	os.MkdirAll(uploadsDir, 0755)

	return &Config{
		Port:            port,
		JWTSecret:       jwtSecret,
		FirebaseCredFile: credFile,
		UploadsDir:      uploadsDir,
	}
}
