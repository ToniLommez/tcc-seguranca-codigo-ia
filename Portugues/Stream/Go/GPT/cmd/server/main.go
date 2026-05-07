package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"path/filepath"

	"streamapp/internal/api"
	"streamapp/internal/auth"
	"streamapp/internal/config"
	"streamapp/internal/firebase"
	"streamapp/internal/music"
)

func main() {
	cfg := config.Load()

	if err := os.MkdirAll(cfg.UploadDir, 0o755); err != nil {
		log.Fatalf("failed to create upload dir: %v", err)
	}

	if err := os.MkdirAll(filepath.Dir(cfg.SQLitePath), 0o755); err != nil {
		log.Fatalf("failed to create database dir: %v", err)
	}

	ctx := context.Background()

	userRepo, err := firebase.NewUserRepository(ctx, cfg.FirebaseCredentialsPath, cfg.FirestoreProjectID)
	if err != nil {
		log.Fatalf("failed to initialize firebase user repository: %v", err)
	}
	defer userRepo.Close()

	musicRepo, err := music.NewRepository(cfg.SQLitePath)
	if err != nil {
		log.Fatalf("failed to initialize music repository: %v", err)
	}
	defer musicRepo.Close()

	jwtService := auth.NewJWTService(cfg.JWTSecret)
	server := api.NewServer(cfg, jwtService, userRepo, musicRepo)

	log.Printf("server listening on http://localhost:%s", cfg.Port)
	if err := http.ListenAndServe(":"+cfg.Port, server.Router()); err != nil {
		log.Fatalf("server stopped: %v", err)
	}
}
