package main

import (
	"fmt"
	"log"
	"net/http"

	"github.com/gorilla/mux"
	"github.com/rs/cors"

	"music-stream/internal/auth"
	"music-stream/internal/config"
	"music-stream/internal/firebase"
	"music-stream/internal/handlers"
	"music-stream/internal/models"
)

func main() {
	cfg := config.Load()

	fb, err := firebase.NewClient(cfg.FirebaseCredFile)
	if err != nil {
		log.Fatalf("Failed to initialize Firebase: %v", err)
	}
	defer fb.Close()

	authHandler := handlers.NewAuthHandler(fb, cfg)
	artistHandler := handlers.NewArtistHandler(fb, cfg)
	userHandler := handlers.NewUserHandler(fb)
	streamHandler := handlers.NewStreamHandler(fb)

	r := mux.NewRouter()

	api := r.PathPrefix("/api").Subrouter()

	api.HandleFunc("/auth/register", authHandler.Register).Methods("POST")
	api.HandleFunc("/auth/login", authHandler.Login).Methods("POST")

	artistRouter := api.PathPrefix("/artist").Subrouter()
	artistRouter.Use(auth.AuthMiddleware(cfg.JWTSecret))
	artistRouter.Use(auth.RequireRole(models.UserTypeArtist))
	artistRouter.HandleFunc("/songs", artistHandler.UploadSong).Methods("POST")
	artistRouter.HandleFunc("/songs", artistHandler.MySongs).Methods("GET")
	artistRouter.HandleFunc("/songs/{id}", artistHandler.DeleteSong).Methods("DELETE")

	songsRouter := api.PathPrefix("/songs").Subrouter()
	songsRouter.Use(auth.AuthMiddleware(cfg.JWTSecret))
	songsRouter.Use(auth.RequireRole(models.UserTypeUser))
	songsRouter.HandleFunc("", userHandler.ListSongs).Methods("GET")
	songsRouter.HandleFunc("/search", userHandler.SearchSongs).Methods("GET")

	streamRouter := api.PathPrefix("/stream").Subrouter()
	streamRouter.Use(auth.AuthMiddleware(cfg.JWTSecret))
	streamRouter.Use(auth.RequireRole(models.UserTypeUser))
	streamRouter.HandleFunc("/{id}", streamHandler.StreamSong).Methods("GET")

	r.PathPrefix("/").Handler(http.FileServer(http.Dir("frontend")))

	c := cors.New(cors.Options{
		AllowedOrigins:   []string{"*"},
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Authorization", "Content-Type"},
		AllowCredentials: true,
	})

	handler := c.Handler(r)

	fmt.Printf("Server starting on http://localhost:%s\n", cfg.Port)
	fmt.Printf("Frontend available at http://localhost:%s\n", cfg.Port)
	log.Fatal(http.ListenAndServe(":"+cfg.Port, handler))
}
