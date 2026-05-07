package app

import (
	"context"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/go-chi/chi/v5"
	chimiddleware "github.com/go-chi/chi/v5/middleware"
	"github.com/go-chi/cors"

	"leadmanager/internal/config"
	"leadmanager/internal/httpapi"
	authmiddleware "leadmanager/internal/middleware"
	"leadmanager/internal/repository"
	"leadmanager/internal/service"
)

func New(ctx context.Context, cfg config.Config) (*http.Server, error) {
	repo, err := repository.NewFirestoreRepository(ctx, cfg)
	if err != nil {
		return nil, err
	}

	authService := service.NewAuthService(cfg, repo)
	leadService := service.NewLeadService(repo)
	handler := httpapi.NewHandler(authService, leadService)

	router := chi.NewRouter()
	router.Use(chimiddleware.RequestID)
	router.Use(chimiddleware.RealIP)
	router.Use(chimiddleware.Logger)
	router.Use(chimiddleware.Recoverer)
	router.Use(cors.Handler(cors.Options{
		AllowedOrigins:   cfg.AllowedOrigins,
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Accept", "Authorization", "Content-Type"},
		AllowCredentials: true,
		MaxAge:           300,
	}))

	router.Get("/health", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"status":"ok"}`))
	})

	router.Route("/api", func(r chi.Router) {
		r.Route("/auth", func(authRouter chi.Router) {
			handler.RegisterPublicAuthRoutes(authRouter)
			authRouter.Group(func(protected chi.Router) {
				protected.Use(authmiddleware.RequireAuth(authService))
				handler.RegisterProtectedAuthRoutes(protected)
			})
		})

		r.Group(func(protected chi.Router) {
			protected.Use(authmiddleware.RequireAuth(authService))
			handler.RegisterLeadRoutes(protected)
		})
	})

	router.NotFound(spaHandler(cfg.StaticDir))

	server := &http.Server{
		Addr:              ":" + cfg.Port,
		Handler:           fileAwareHandler(router, cfg.StaticDir),
		ReadHeaderTimeout: 10 * time.Second,
	}

	server.RegisterOnShutdown(func() {
		_ = repo.Close()
	})

	return server, nil
}

func fileAwareHandler(router http.Handler, staticDir string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if strings.HasPrefix(r.URL.Path, "/api") || r.URL.Path == "/health" {
			router.ServeHTTP(w, r)
			return
		}

		target := filepath.Join(staticDir, filepath.Clean(strings.TrimPrefix(r.URL.Path, "/")))
		if info, err := os.Stat(target); err == nil && !info.IsDir() {
			http.ServeFile(w, r, target)
			return
		}

		router.ServeHTTP(w, r)
	})
}

func spaHandler(staticDir string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		indexPath := filepath.Join(staticDir, "index.html")
		if _, err := os.Stat(indexPath); err != nil {
			http.Error(w, "frontend not found", http.StatusInternalServerError)
			return
		}

		http.ServeFile(w, r, indexPath)
	}
}
