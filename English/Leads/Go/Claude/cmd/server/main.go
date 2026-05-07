package main

import (
	"log"
	"net/http"
	"os"
	"strings"

	fb "lead-manager/internal/firebase"
	"lead-manager/internal/handlers"
	"lead-manager/internal/middleware"
)

func main() {
	credPath := os.Getenv("FIREBASE_CREDENTIALS")
	if credPath == "" {
		credPath = `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`
	}

	fb.InitFirestore(credPath)
	log.Println("Firebase connected")

	authH := handlers.NewAuthHandler()
	leadH := handlers.NewLeadHandler()
	ieH := handlers.NewImportExportHandler()

	mux := http.NewServeMux()

	// Static files
	mux.Handle("/static/", http.StripPrefix("/static/", http.FileServer(http.Dir("web/static"))))

	// HTML pages
	mux.HandleFunc("/", serveTemplate("web/templates/index.html"))
	mux.HandleFunc("/dashboard", serveTemplate("web/templates/dashboard.html"))
	mux.HandleFunc("/leads", serveLeadsPage)

	// Auth API
	mux.HandleFunc("/api/auth/signup", authH.Signup)
	mux.HandleFunc("/api/auth/login", authH.Login)

	// Protected lead API routes
	protected := func(h http.HandlerFunc) http.Handler {
		return middleware.AuthMiddleware(http.HandlerFunc(h))
	}

	// Export routes — token via query param for direct browser download
	mux.Handle("/api/leads/export/csv", tokenOrBearerAuth(http.HandlerFunc(ieH.ExportCSV)))
	mux.Handle("/api/leads/export/excel", tokenOrBearerAuth(http.HandlerFunc(ieH.ExportExcel)))

	mux.Handle("/api/leads/import", protected(ieH.Import))
	mux.Handle("/api/leads/stats", protected(leadH.GetStats))
	mux.Handle("/api/leads", protected(leadsRouter(leadH)))
	mux.Handle("/api/leads/", protected(leadDetailRouter(leadH)))

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	log.Printf("Server starting on http://localhost:%s", port)
	if err := http.ListenAndServe(":"+port, middleware.CORS(mux)); err != nil {
		log.Fatalf("Server failed: %v", err)
	}
}

func serveTemplate(path string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" && !strings.HasSuffix(r.URL.Path, ".html") && r.URL.Path != "/dashboard" {
			http.NotFound(w, r)
			return
		}
		http.ServeFile(w, r, path)
	}
}

func serveLeadsPage(w http.ResponseWriter, r *http.Request) {
	// /leads/<id> serves the detail page; /leads serves the list
	path := strings.TrimPrefix(r.URL.Path, "/leads")
	if path == "" || path == "/" {
		http.ServeFile(w, r, "web/templates/leads.html")
		return
	}
	// strip leading slash to get id
	id := strings.TrimPrefix(path, "/")
	if id != "" && !strings.Contains(id, "/") {
		http.ServeFile(w, r, "web/templates/lead-detail.html")
		return
	}
	http.NotFound(w, r)
}

func leadsRouter(h *handlers.LeadHandler) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			h.GetAll(w, r)
		case http.MethodPost:
			h.Create(w, r)
		default:
			http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		}
	}
}

func leadDetailRouter(h *handlers.LeadHandler) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		path := strings.TrimPrefix(r.URL.Path, "/api/leads/")
		parts := strings.Split(strings.Trim(path, "/"), "/")

		// /api/leads/{id}/interactions
		if len(parts) == 2 && parts[1] == "interactions" {
			switch r.Method {
			case http.MethodGet:
				h.GetInteractions(w, r)
			case http.MethodPost:
				h.AddInteraction(w, r)
			default:
				http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
			}
			return
		}

		// /api/leads/{id}
		switch r.Method {
		case http.MethodGet:
			h.GetByID(w, r)
		case http.MethodPut:
			h.Update(w, r)
		case http.MethodDelete:
			h.Delete(w, r)
		default:
			http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		}
	}
}

// tokenOrBearerAuth allows token via ?token= query param (for direct browser downloads)
// or standard Bearer header.
func tokenOrBearerAuth(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if token := r.URL.Query().Get("token"); token != "" {
			r.Header.Set("Authorization", "Bearer "+token)
		}
		middleware.AuthMiddleware(next).ServeHTTP(w, r)
	})
}
