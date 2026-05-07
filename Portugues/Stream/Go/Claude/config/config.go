package config

import (
	"context"
	"os"

	firebase "firebase.google.com/go/v4"
	"google.golang.org/api/option"
)

const defaultCredentialsPath = `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`

var JWTSecret = getEnv("JWT_SECRET", "musicstream-jwt-secret-change-in-production")

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func InitFirebase() (*firebase.App, error) {
	credPath := os.Getenv("FIREBASE_CREDENTIALS")
	if credPath == "" {
		credPath = defaultCredentialsPath
	}

	opt := option.WithCredentialsFile(credPath)
	app, err := firebase.NewApp(context.Background(), nil, opt)
	if err != nil {
		return nil, err
	}
	return app, nil
}
