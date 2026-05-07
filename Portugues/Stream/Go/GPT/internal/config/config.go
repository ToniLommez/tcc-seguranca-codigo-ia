package config

import "os"

type Config struct {
	Port                    string
	JWTSecret               string
	FirebaseCredentialsPath string
	FirestoreProjectID      string
	SQLitePath              string
	UploadDir               string
	WebDir                  string
}

func Load() Config {
	return Config{
		Port:                    getEnv("APP_PORT", "8080"),
		JWTSecret:               getEnv("JWT_SECRET", "change-this-secret-in-production"),
		FirebaseCredentialsPath: getEnv("FIREBASE_CREDENTIALS_PATH", `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`),
		FirestoreProjectID:      os.Getenv("FIREBASE_PROJECT_ID"),
		SQLitePath:              getEnv("SQLITE_PATH", "./data/stream.db"),
		UploadDir:               getEnv("UPLOAD_DIR", "./uploads"),
		WebDir:                  getEnv("WEB_DIR", "./web"),
	}
}

func getEnv(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}

	return fallback
}
