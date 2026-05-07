package main

import (
	"context"
	"log"

	"leadmanager/internal/app"
	"leadmanager/internal/config"
)

func main() {
	cfg := config.Load()

	server, err := app.New(context.Background(), cfg)
	if err != nil {
		log.Fatalf("failed to initialize app: %v", err)
	}

	log.Printf("lead manager listening on http://localhost:%s", cfg.Port)
	if err := server.ListenAndServe(); err != nil {
		log.Fatalf("server exited: %v", err)
	}
}
