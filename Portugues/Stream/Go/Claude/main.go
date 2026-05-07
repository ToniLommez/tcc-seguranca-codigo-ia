package main

import (
	"log"
	"net/http"
	"os"

	"github.com/gorilla/mux"

	"musicstream/config"
	"musicstream/db"
	"musicstream/handlers"
	"musicstream/middleware"
	"musicstream/models"
)

func main() {
	fbApp, err := config.InitFirebase()
	if err != nil {
		log.Fatal("Falha ao inicializar Firebase:", err)
	}

	database, err := db.InitSQLite()
	if err != nil {
		log.Fatal("Falha ao inicializar SQLite:", err)
	}
	defer database.Close()

	if err := os.MkdirAll("uploads", 0755); err != nil {
		log.Fatal("Falha ao criar diretório de uploads:", err)
	}

	authHandler := handlers.NewAuthHandler(fbApp)
	artistHandler := handlers.NewArtistHandler(database)
	musicHandler := handlers.NewMusicHandler(database)

	r := mux.NewRouter()

	// Public auth routes.
	auth := r.PathPrefix("/api/auth").Subrouter()
	auth.HandleFunc("/register", authHandler.Register).Methods(http.MethodPost)
	auth.HandleFunc("/login", authHandler.Login).Methods(http.MethodPost)

	// Artist-only routes.
	artist := r.PathPrefix("/api/artist").Subrouter()
	artist.Use(middleware.JWT, middleware.RequireRole(models.TypeArtist))
	artist.HandleFunc("/music", artistHandler.UploadMusic).Methods(http.MethodPost)
	artist.HandleFunc("/music", artistHandler.ListMyMusic).Methods(http.MethodGet)

	// User-only routes.
	user := r.PathPrefix("/api").Subrouter()
	user.Use(middleware.JWT, middleware.RequireRole(models.TypeUser))
	user.HandleFunc("/music", musicHandler.ListMusic).Methods(http.MethodGet)
	user.HandleFunc("/music/search", musicHandler.SearchMusic).Methods(http.MethodGet)
	user.HandleFunc("/music/{id}/stream", musicHandler.StreamMusic).Methods(http.MethodGet)

	// Frontend static files.
	r.PathPrefix("/").Handler(http.FileServer(http.Dir("frontend")))

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	log.Printf("Servidor rodando em http://localhost:%s", port)
	log.Fatal(http.ListenAndServe(":"+port, r))
}
