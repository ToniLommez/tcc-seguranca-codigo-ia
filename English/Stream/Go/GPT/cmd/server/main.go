package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"streammusic/internal/config"
	"streammusic/internal/database"
	"streammusic/internal/handlers"
	"streammusic/internal/repository"
	"streammusic/internal/router"
	"streammusic/internal/storage"
	"syscall"
	"time"
)

func main() {
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("failed to load configuration: %v", err)
	}

	sqliteDB, err := database.OpenSQLite(cfg.SongsDBPath)
	if err != nil {
		log.Fatalf("failed to open songs database: %v", err)
	}
	defer sqliteDB.Close()

	ctx := context.Background()
	userRepo, err := repository.NewFirestoreUserRepository(ctx, cfg.FirebaseProjectID, cfg.FirebaseCredentials)
	if err != nil {
		log.Fatalf("failed to connect to firestore: %v", err)
	}
	defer userRepo.Close()

	songRepo := repository.NewSongRepository(sqliteDB)
	fileStorage := storage.NewLocalMusicStorage(cfg.UploadDir)

	authHandler := handlers.NewAuthHandler(cfg, userRepo)
	artistHandler := handlers.NewArtistHandler(songRepo, fileStorage)
	userHandler := handlers.NewUserHandler(songRepo)

	engine := router.Setup(cfg, authHandler, artistHandler, userHandler)

	server := &http.Server{
		Addr:              ":" + cfg.Port,
		Handler:           engine,
		ReadHeaderTimeout: 10 * time.Second,
	}

	go func() {
		log.Printf("server listening on http://localhost:%s", cfg.Port)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("server failed: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := server.Shutdown(shutdownCtx); err != nil {
		log.Printf("graceful shutdown failed: %v", err)
	}
}
