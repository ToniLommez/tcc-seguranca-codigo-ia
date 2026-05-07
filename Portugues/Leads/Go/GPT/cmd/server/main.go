package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"leadmanagergpt/internal/config"
	webapp "leadmanagergpt/internal/http"
	"leadmanagergpt/internal/service"
	"leadmanagergpt/internal/store"
)

func main() {
	cfg := config.Load()

	ctx := context.Background()
	firestoreStore, err := store.NewFirestoreStore(ctx, cfg)
	if err != nil {
		log.Fatalf("failed to initialize firestore: %v", err)
	}
	defer firestoreStore.Close()

	authService := service.NewAuthService(firestoreStore, cfg)
	leadService := service.NewLeadService(firestoreStore)

	server, err := webapp.NewServer(cfg, authService, leadService)
	if err != nil {
		log.Fatalf("failed to initialize server: %v", err)
	}

	httpServer := &http.Server{
		Addr:              ":" + cfg.Port,
		Handler:           server.Router(),
		ReadHeaderTimeout: 10 * time.Second,
		ReadTimeout:       30 * time.Second,
		WriteTimeout:      30 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	go func() {
		log.Printf("Lead Manager running at http://localhost:%s", cfg.Port)
		if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("server error: %v", err)
		}
	}()

	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
	<-stop

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := httpServer.Shutdown(shutdownCtx); err != nil {
		log.Printf("graceful shutdown failed: %v", err)
	}
}
