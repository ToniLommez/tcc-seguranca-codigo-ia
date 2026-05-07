package webapp

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"html/template"
	"io"
	"io/fs"
	"net/http"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/go-chi/chi/v5"
	chimiddleware "github.com/go-chi/chi/v5/middleware"

	"leadmanagergpt/internal/config"
	"leadmanagergpt/internal/models"
	"leadmanagergpt/internal/service"
	"leadmanagergpt/internal/store"
	webassets "leadmanagergpt/web"
)

type contextKey string

const userContextKey contextKey = "authenticated_user"

type Server struct {
	config    config.Config
	auth      *service.AuthService
	leads     *service.LeadService
	templates *template.Template
}

type TemplateData struct {
	Title        string
	AppName      string
	User         models.User
	Lead         models.Lead
	Leads        []models.Lead
	Paginated    models.PaginatedLeads
	Interactions []models.Interaction
	Dashboard    models.DashboardStats
	Params       models.LeadFilterParams
	Sources      []string
	Statuses     []string
	PageNumbers  []int
	Mode         string
}

func NewServer(cfg config.Config, auth *service.AuthService, leads *service.LeadService) (*Server, error) {
	funcMap := template.FuncMap{
		"formatDate": func(value time.Time) string {
			if value.IsZero() {
				return "-"
			}
			return value.Local().Format("02/01/2006")
		},
		"formatDateTime": func(value time.Time) string {
			if value.IsZero() {
				return "-"
			}
			return value.Local().Format("02/01/2006 15:04")
		},
		"statusClass": func(status string) string {
			return strings.ReplaceAll(strings.ToLower(status), " ", "-")
		},
		"inc": func(value int) int { return value + 1 },
		"dec": func(value int) int {
			if value <= 1 {
				return 1
			}
			return value - 1
		},
	}

	templates, err := template.New("").Funcs(funcMap).ParseFS(webassets.Files, "templates/*.html")
	if err != nil {
		return nil, fmt.Errorf("parse templates: %w", err)
	}

	return &Server{
		config:    cfg,
		auth:      auth,
		leads:     leads,
		templates: templates,
	}, nil
}

func (s *Server) Router() http.Handler {
	router := chi.NewRouter()
	router.Use(chimiddleware.RequestID)
	router.Use(chimiddleware.RealIP)
	router.Use(chimiddleware.Recoverer)
	router.Use(chimiddleware.Logger)

	staticFS, _ := fs.Sub(webassets.Files, "static")
	router.Handle("/static/*", http.StripPrefix("/static/", http.FileServer(http.FS(staticFS))))

	router.Get("/", s.handleHome)
	router.Get("/login", s.handleLoginPage)
	router.Get("/register", s.handleRegisterPage)

	router.Route("/api", func(r chi.Router) {
		r.Post("/auth/register", s.handleRegisterAPI)
		r.Post("/auth/login", s.handleLoginAPI)
		r.Post("/auth/logout", s.handleLogoutAPI)

		r.Group(func(protected chi.Router) {
			protected.Use(s.apiAuthMiddleware)
			protected.Get("/me", s.handleMeAPI)
			protected.Get("/dashboard", s.handleDashboardAPI)
			protected.Get("/leads", s.handleListLeadsAPI)
			protected.Post("/leads", s.handleCreateLeadAPI)
			protected.Post("/leads/import", s.handleImportLeadsAPI)
			protected.Get("/leads/export", s.handleExportLeadsAPI)
			protected.Get("/leads/{leadID}", s.handleGetLeadAPI)
			protected.Put("/leads/{leadID}", s.handleUpdateLeadAPI)
			protected.Delete("/leads/{leadID}", s.handleDeleteLeadAPI)
			protected.Get("/leads/{leadID}/interactions", s.handleListInteractionsAPI)
			protected.Post("/leads/{leadID}/interactions", s.handleAddInteractionAPI)
			protected.Delete("/leads/{leadID}/interactions/{interactionID}", s.handleDeleteInteractionAPI)
		})
	})

	router.Group(func(app chi.Router) {
		app.Use(s.pageAuthMiddleware)
		app.Get("/app/dashboard", s.handleDashboardPage)
		app.Get("/app/leads", s.handleLeadsPage)
		app.Get("/app/leads/new", s.handleLeadCreatePage)
		app.Get("/app/leads/{leadID}", s.handleLeadDetailPage)
		app.Get("/app/leads/{leadID}/edit", s.handleLeadEditPage)
	})

	return router
}

func (s *Server) handleHome(w http.ResponseWriter, r *http.Request) {
	if _, err := s.userFromRequest(r); err == nil {
		http.Redirect(w, r, "/app/dashboard", http.StatusFound)
		return
	}
	http.Redirect(w, r, "/login", http.StatusFound)
}

func (s *Server) handleLoginPage(w http.ResponseWriter, r *http.Request) {
	if _, err := s.userFromRequest(r); err == nil {
		http.Redirect(w, r, "/app/dashboard", http.StatusFound)
		return
	}
	s.render(w, "login.html", TemplateData{Title: "Entrar", AppName: s.config.AppName})
}

func (s *Server) handleRegisterPage(w http.ResponseWriter, r *http.Request) {
	if _, err := s.userFromRequest(r); err == nil {
		http.Redirect(w, r, "/app/dashboard", http.StatusFound)
		return
	}
	s.render(w, "register.html", TemplateData{Title: "Criar Conta", AppName: s.config.AppName})
}

func (s *Server) handleDashboardPage(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)

	stats, err := s.leads.Dashboard(r.Context(), user.ID)
	if err != nil {
		s.serverError(w, err)
		return
	}

	recent, err := s.leads.ListLeads(r.Context(), user.ID, models.LeadFilterParams{
		SortBy:        "capture_date",
		SortDirection: "desc",
		Page:          1,
		PageSize:      8,
	})
	if err != nil {
		s.serverError(w, err)
		return
	}

	s.render(w, "dashboard.html", TemplateData{
		Title:     "Dashboard",
		AppName:   s.config.AppName,
		User:      user,
		Dashboard: stats,
		Leads:     recent.Items,
		Statuses:  leadStatuses(),
	})
}

func (s *Server) handleLeadsPage(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	params := filterParamsFromRequest(r)

	paginated, err := s.leads.ListLeads(r.Context(), user.ID, params)
	if err != nil {
		s.serverError(w, err)
		return
	}

	allLeads, err := s.leads.ListLeadsNoPagination(r.Context(), user.ID, models.LeadFilterParams{})
	if err != nil {
		s.serverError(w, err)
		return
	}

	s.render(w, "leads.html", TemplateData{
		Title:       "Leads",
		AppName:     s.config.AppName,
		User:        user,
		Paginated:   paginated,
		Params:      params,
		Sources:     uniqueSources(allLeads),
		Statuses:    leadStatuses(),
		PageNumbers: buildPageNumbers(paginated.TotalPages),
	})
}

func (s *Server) handleLeadCreatePage(w http.ResponseWriter, r *http.Request) {
	s.render(w, "lead_form.html", TemplateData{
		Title:    "Novo Lead",
		AppName:  s.config.AppName,
		User:     authenticatedUser(r),
		Statuses: leadStatuses(),
		Mode:     "create",
	})
}

func (s *Server) handleLeadEditPage(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	lead, err := s.leads.GetLead(r.Context(), user.ID, chi.URLParam(r, "leadID"))
	if err != nil {
		s.handleDomainError(w, r, err)
		return
	}

	s.render(w, "lead_form.html", TemplateData{
		Title:    "Editar Lead",
		AppName:  s.config.AppName,
		User:     user,
		Lead:     lead,
		Statuses: leadStatuses(),
		Mode:     "edit",
	})
}

func (s *Server) handleLeadDetailPage(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	leadID := chi.URLParam(r, "leadID")

	lead, err := s.leads.GetLead(r.Context(), user.ID, leadID)
	if err != nil {
		s.handleDomainError(w, r, err)
		return
	}
	interactions, err := s.leads.ListInteractions(r.Context(), user.ID, leadID)
	if err != nil {
		s.handleDomainError(w, r, err)
		return
	}

	s.render(w, "lead_detail.html", TemplateData{
		Title:        "Detalhes do Lead",
		AppName:      s.config.AppName,
		User:         user,
		Lead:         lead,
		Interactions: interactions,
		Statuses:     leadStatuses(),
	})
}

func (s *Server) handleRegisterAPI(w http.ResponseWriter, r *http.Request) {
	var payload struct {
		Name     string `json:"name"`
		Email    string `json:"email"`
		Password string `json:"password"`
	}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		s.badRequest(w, "payload invalido")
		return
	}

	user, token, err := s.auth.Register(r.Context(), payload.Name, payload.Email, payload.Password)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}

	s.setAuthCookie(w, token)
	s.writeJSON(w, http.StatusCreated, map[string]any{
		"user":  user,
		"token": token,
	})
}

func (s *Server) handleLoginAPI(w http.ResponseWriter, r *http.Request) {
	var payload struct {
		Email    string `json:"email"`
		Password string `json:"password"`
	}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		s.badRequest(w, "payload invalido")
		return
	}

	user, token, err := s.auth.Login(r.Context(), payload.Email, payload.Password)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}

	s.setAuthCookie(w, token)
	s.writeJSON(w, http.StatusOK, map[string]any{
		"user":  user,
		"token": token,
	})
}

func (s *Server) handleLogoutAPI(w http.ResponseWriter, r *http.Request) {
	s.clearAuthCookie(w)
	s.writeJSON(w, http.StatusOK, map[string]string{"message": "logout realizado"})
}

func (s *Server) handleMeAPI(w http.ResponseWriter, r *http.Request) {
	s.writeJSON(w, http.StatusOK, authenticatedUser(r))
}

func (s *Server) handleDashboardAPI(w http.ResponseWriter, r *http.Request) {
	stats, err := s.leads.Dashboard(r.Context(), authenticatedUser(r).ID)
	if err != nil {
		s.serverError(w, err)
		return
	}
	s.writeJSON(w, http.StatusOK, stats)
}

func (s *Server) handleListLeadsAPI(w http.ResponseWriter, r *http.Request) {
	params := filterParamsFromRequest(r)
	leads, err := s.leads.ListLeads(r.Context(), authenticatedUser(r).ID, params)
	if err != nil {
		s.serverError(w, err)
		return
	}
	s.writeJSON(w, http.StatusOK, leads)
}

func (s *Server) handleCreateLeadAPI(w http.ResponseWriter, r *http.Request) {
	var input models.LeadInput
	if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
		s.badRequest(w, "payload invalido")
		return
	}

	user := authenticatedUser(r)
	lead, err := s.leads.CreateLead(r.Context(), user.ID, user.Name, input)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	s.writeJSON(w, http.StatusCreated, lead)
}

func (s *Server) handleGetLeadAPI(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	leadID := chi.URLParam(r, "leadID")

	lead, err := s.leads.GetLead(r.Context(), user.ID, leadID)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	interactions, err := s.leads.ListInteractions(r.Context(), user.ID, leadID)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}

	s.writeJSON(w, http.StatusOK, map[string]any{
		"lead":         lead,
		"interactions": interactions,
	})
}

func (s *Server) handleUpdateLeadAPI(w http.ResponseWriter, r *http.Request) {
	var input models.LeadInput
	if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
		s.badRequest(w, "payload invalido")
		return
	}

	user := authenticatedUser(r)
	lead, err := s.leads.UpdateLead(r.Context(), user.ID, chi.URLParam(r, "leadID"), user.Name, input)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}

	s.writeJSON(w, http.StatusOK, lead)
}

func (s *Server) handleDeleteLeadAPI(w http.ResponseWriter, r *http.Request) {
	err := s.leads.DeleteLead(r.Context(), authenticatedUser(r).ID, chi.URLParam(r, "leadID"))
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	s.writeJSON(w, http.StatusOK, map[string]string{"message": "lead removido"})
}

func (s *Server) handleListInteractionsAPI(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	interactions, err := s.leads.ListInteractions(r.Context(), user.ID, chi.URLParam(r, "leadID"))
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	s.writeJSON(w, http.StatusOK, interactions)
}

func (s *Server) handleAddInteractionAPI(w http.ResponseWriter, r *http.Request) {
	var input models.InteractionInput
	if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
		s.badRequest(w, "payload invalido")
		return
	}

	user := authenticatedUser(r)
	interaction, err := s.leads.AddInteraction(r.Context(), user.ID, chi.URLParam(r, "leadID"), user.Name, input)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	s.writeJSON(w, http.StatusCreated, interaction)
}

func (s *Server) handleDeleteInteractionAPI(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	err := s.leads.DeleteInteraction(r.Context(), user.ID, chi.URLParam(r, "leadID"), chi.URLParam(r, "interactionID"))
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	s.writeJSON(w, http.StatusOK, map[string]string{"message": "interacao removida"})
}

func (s *Server) handleImportLeadsAPI(w http.ResponseWriter, r *http.Request) {
	if err := r.ParseMultipartForm(20 << 20); err != nil {
		s.badRequest(w, "arquivo invalido")
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		s.badRequest(w, "arquivo nao enviado")
		return
	}
	defer file.Close()

	user := authenticatedUser(r)
	result, err := s.leads.ImportLeads(r.Context(), user.ID, user.Name, header.Filename, file)
	if err != nil {
		s.handleDomainErrorJSON(w, err)
		return
	}
	s.writeJSON(w, http.StatusOK, result)
}

func (s *Server) handleExportLeadsAPI(w http.ResponseWriter, r *http.Request) {
	user := authenticatedUser(r)
	params := filterParamsFromRequest(r)
	format := strings.TrimSpace(r.URL.Query().Get("format"))

	payload, contentType, fileName, err := s.leads.ExportLeads(r.Context(), user.ID, params, format)
	if err != nil {
		s.serverError(w, err)
		return
	}

	w.Header().Set("Content-Type", contentType)
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=%q", fileName))
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write(payload)
}

func (s *Server) render(w http.ResponseWriter, templateName string, data TemplateData) {
	if err := s.templates.ExecuteTemplate(w, templateName, data); err != nil {
		s.serverError(w, err)
	}
}

func (s *Server) writeJSON(w http.ResponseWriter, status int, payload any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(payload)
}

func (s *Server) badRequest(w http.ResponseWriter, message string) {
	s.writeJSON(w, http.StatusBadRequest, map[string]string{"error": message})
}

func (s *Server) serverError(w http.ResponseWriter, err error) {
	http.Error(w, "erro interno do servidor", http.StatusInternalServerError)
}

func (s *Server) handleDomainError(w http.ResponseWriter, r *http.Request, err error) {
	switch {
	case errors.Is(err, store.ErrNotFound):
		http.NotFound(w, r)
	default:
		http.Error(w, err.Error(), http.StatusBadRequest)
	}
}

func (s *Server) handleDomainErrorJSON(w http.ResponseWriter, err error) {
	status := http.StatusBadRequest
	switch {
	case errors.Is(err, store.ErrNotFound):
		status = http.StatusNotFound
	case errors.Is(err, service.ErrInvalidCredentials):
		status = http.StatusUnauthorized
	case errors.Is(err, service.ErrEmailInUse):
		status = http.StatusConflict
	}
	s.writeJSON(w, status, map[string]string{"error": err.Error()})
}

func (s *Server) setAuthCookie(w http.ResponseWriter, token string) {
	http.SetCookie(w, &http.Cookie{
		Name:     s.config.CookieName,
		Value:    token,
		HttpOnly: true,
		Path:     "/",
		SameSite: http.SameSiteLaxMode,
		MaxAge:   s.config.JWTExpiryHours * 3600,
	})
}

func (s *Server) clearAuthCookie(w http.ResponseWriter) {
	http.SetCookie(w, &http.Cookie{
		Name:     s.config.CookieName,
		Value:    "",
		HttpOnly: true,
		Path:     "/",
		SameSite: http.SameSiteLaxMode,
		MaxAge:   -1,
	})
}

func (s *Server) apiAuthMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		user, err := s.userFromRequest(r)
		if err != nil {
			s.writeJSON(w, http.StatusUnauthorized, map[string]string{"error": "nao autenticado"})
			return
		}
		next.ServeHTTP(w, r.WithContext(context.WithValue(r.Context(), userContextKey, user)))
	})
}

func (s *Server) pageAuthMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		user, err := s.userFromRequest(r)
		if err != nil {
			http.Redirect(w, r, "/login", http.StatusFound)
			return
		}
		next.ServeHTTP(w, r.WithContext(context.WithValue(r.Context(), userContextKey, user)))
	})
}

func (s *Server) userFromRequest(r *http.Request) (models.User, error) {
	token := tokenFromRequest(r, s.config.CookieName)
	if token == "" {
		return models.User{}, service.ErrInvalidCredentials
	}

	claims, err := s.auth.ParseToken(token)
	if err != nil {
		return models.User{}, err
	}

	return s.auth.GetUserByID(r.Context(), claims.UserID)
}

func tokenFromRequest(r *http.Request, cookieName string) string {
	header := strings.TrimSpace(r.Header.Get("Authorization"))
	if strings.HasPrefix(strings.ToLower(header), "bearer ") {
		return strings.TrimSpace(header[7:])
	}

	cookie, err := r.Cookie(cookieName)
	if err == nil {
		return cookie.Value
	}
	return ""
}

func authenticatedUser(r *http.Request) models.User {
	user, _ := r.Context().Value(userContextKey).(models.User)
	return user
}

func filterParamsFromRequest(r *http.Request) models.LeadFilterParams {
	query := r.URL.Query()
	return models.LeadFilterParams{
		Query:           query.Get("q"),
		Status:          query.Get("status"),
		Source:          query.Get("source"),
		CaptureDateFrom: query.Get("capture_date_from"),
		CaptureDateTo:   query.Get("capture_date_to"),
		SortBy:          query.Get("sort_by"),
		SortDirection:   query.Get("sort_direction"),
		Page:            parseInt(query.Get("page"), 1),
		PageSize:        parseInt(query.Get("page_size"), 10),
	}
}

func parseInt(raw string, fallback int) int {
	value, err := strconv.Atoi(strings.TrimSpace(raw))
	if err != nil {
		return fallback
	}
	return value
}

func uniqueSources(leads []models.Lead) []string {
	set := map[string]struct{}{}
	for _, lead := range leads {
		source := strings.TrimSpace(lead.Source)
		if source != "" {
			set[source] = struct{}{}
		}
	}

	sources := make([]string, 0, len(set))
	for source := range set {
		sources = append(sources, source)
	}
	sort.Strings(sources)
	return sources
}

func buildPageNumbers(totalPages int) []int {
	if totalPages < 1 {
		return []int{1}
	}

	pages := make([]int, 0, totalPages)
	for i := 1; i <= totalPages; i++ {
		pages = append(pages, i)
	}
	return pages
}

func leadStatuses() []string {
	return []string{
		models.LeadStatusNew,
		models.LeadStatusContacted,
		models.LeadStatusQualified,
		models.LeadStatusLost,
	}
}

func bodyToBytes(body io.ReadCloser) []byte {
	defer body.Close()
	data, _ := io.ReadAll(body)
	return data
}
